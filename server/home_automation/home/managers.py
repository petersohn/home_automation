import home.models
from home import config

import django.utils.timezone
from django.db import models

import datetime


class DeviceManager(models.Manager):
    def count_alive(self):
        return self.get_alive().count()

    def get_alive(self):
        return self.filter(
            last_seen__gte=home.models.Device.heartbeat_time_limit())

    def count_dead(self):
        return self.filter(
            last_seen__lt=home.models.Device.heartbeat_time_limit()).count()


