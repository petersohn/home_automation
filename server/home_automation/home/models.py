import inspect
import home.config
from django.db import models
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
    text = models.TextField(null=False)


class Device(models.Model):
    name = models.CharField(max_length=200, null=False, db_index=True)
    ip_address = models.GenericIPAddressField(null=False, protocol='both')
    port = models.PositiveIntegerField(null=False)
    version = models.PositiveIntegerField(null=False)
    # FIXME: auto_now?
    last_seen = models.DateTimeField(null=False, auto_now=True)

    def is_alive(self):
        return self.last_seen >= config.device_heartbeat_timeout

class Pin(models.Model):
    class Kind(ChoiceEnum):
        INPUT = 0
        OUTPUT = 1
    device = models.ForeignKey(Device, null=False, on_delete=models.CASCADE)
    name = models.CharField(null=False, max_length=200, db_index=True)
    kind = models.PositiveSmallIntegerField(null=False, choices=Kind.choices())
    expression = models.ForeignKey(Expression, null=True, on_delete=models.SET_NULL)
    # FIXME: unique?


class Variable(models.Model):
    name = models.CharField(null=False, max_length=200, db_index=True)
    value = models.IntegerField(null=False)

    def get(self):
        return self.value

    def set(self, value):
        self.value = value
        self.save() # FIXME: save?

    def toggle(self, modulo=2):
        self.value = (self.value + 1) % modulo
        self.save() # FIXME: save?


class InputTrigger(models.Model):
    class Edge(ChoiceEnum):
        RISING = 0
        FALLING = 1
        BOTH = 2
    pin = models.ForeignKey(Pin, null=False, on_delete=models.CASCADE, db_index=True)
    expression = models.ForeignKey(Expression, null=False, on_delete=models.CASCADE)
    edge = models.PositiveSmallIntegerField(null=False, choices=Edge.choices())


class Log(models.Model):
    class Severity(ChoiceEnum):
        INFO = 0
        WARNING = 1
        ERROR = 2
    device = models.ForeignKey(Device, null=True, on_delete=models.SET_NULL)
    pin = models.ForeignKey(Pin, null=True, on_delete=models.SET_NULL)
    severity = models.PositiveSmallIntegerField(null=False, choices=Severity.choices())
    time = models.DateTimeField(null=False, auto_now_add=True)
    mesasge = models.TextField(null=False)
