import home.config as config
from home.managers import DeviceManager

import django.utils.timezone
from django.db import models

import inspect
from enum import Enum


class ChoiceEnum(Enum):
    @classmethod
    def choices(cls):
        # get all members of the class
        members = inspect.getmembers(cls, lambda m: not(inspect.isroutine(m)))
        # filter down to just properties
        props = [m for m in members if not(m[0][:2] == '__')]
        # format into django choice tuple
        choices = tuple([(str(p[1].value), p[0]) for p in props])
        return choices


class Expression(models.Model):
    value = models.TextField(null=False)

    def __str__(self):
        return self.value


class Device(models.Model):
    name = models.CharField(max_length=200, null=False, db_index=True,
                            unique=True)
    ip_address = models.GenericIPAddressField(null=False, protocol='both')
    port = models.PositiveIntegerField(null=False)
    version = models.PositiveIntegerField(null=False)
    last_seen = models.DateTimeField(null=False, auto_now=True)

    objects = DeviceManager()

    def is_alive(self):
        return self.last_seen >= self.heartbeat_time_limit()

    @staticmethod
    def heartbeat_time_limit():
        now = django.utils.timezone.now()
        return now - config.device_heartbeat_timeout

    @staticmethod
    def create_from_json(data):
        device = Device.objects.create(
            name=data['name'], ip_address=data['ip'], port=data['port'],
            version=data.get('version', 1))
        return device

    def update_from_json(self, data):
        self.ip_address = data['ip']
        self.port = data['port']
        self.version = data.get('version', 1)
        self.save()

    def __str__(self):
        result = self.name
        if not self.is_alive():
            result += " [dead]"
        return result


class Pin(models.Model):
    class Kind(ChoiceEnum):
        INPUT = 'input'
        OUTPUT = 'output'

        @staticmethod
        def from_string(string_value):
            if string_value == 'input':
                return Pin.Kind.INPUT
            elif string_value == 'output':
                return Pin.Kind.OUTPUT
            raise ValueError('Not a valid Pin Kind')

    device = models.ForeignKey(Device, null=False, on_delete=models.CASCADE)
    name = models.CharField(null=False, max_length=200, db_index=True)
    kind = models.CharField(max_length=10, null=False, choices=Kind.choices())
    expression = models.ForeignKey(Expression, null=True,
                                   on_delete=models.SET_NULL)

    class Meta:
        unique_together = ('device', 'name')

    def __str__(self):
        result = (self.device.name + "." + self.name + " (" + str(self.kind) +
                  ")")
        if self.expression is not None:
            result += " [" + str(self.expression) + "]"
        return result


class Variable(models.Model):
    name = models.CharField(null=False, max_length=200, db_index=True,
                            unique=True)
    value = models.IntegerField(null=False)

    def get(self):
        return self.value

    def set(self, value):
        self.value = value
        self.save()
        return self.value

    def toggle(self, modulo=2):
        self.value = (self.value + 1) % modulo
        self.save()
        return self.value

    def __str__(self):
        return self.name + " (" + str(self.value) + ")"


class InputTrigger(models.Model):
    class Edge(ChoiceEnum):
        RISING = 'rising'
        FALLING = 'falling'
        BOTH = 'both'
    pin = models.ForeignKey(
        Pin, null=False, on_delete=models.CASCADE, db_index=True)
    expression = models.ForeignKey(
        Expression, null=False, on_delete=models.CASCADE)
    edge = models.CharField(max_length=10, null=False, choices=Edge.choices())

    def __str__(self):
        return (str(self.pin) + " --> " + "[" + str(self.expression) +
                "] (" + str(self.edge) + ")")


class Log(models.Model):
    class Severity(ChoiceEnum):
        INFO = 'INFO'
        WARNING = 'WARNING'
        ERROR = 'ERROR'
    device = models.ForeignKey(Device, null=True, on_delete=models.SET_NULL)
    pin = models.ForeignKey(Pin, null=True, on_delete=models.SET_NULL)
    severity = models.CharField(
        max_length=10, null=False, choices=Severity.choices())
    time = models.DateTimeField(null=False, auto_now_add=True)
    message = models.TextField(null=False)

    def __str__(self):
        return (str(self.time) + ": [" + str(self.severity) + "] " +
                self.message)
