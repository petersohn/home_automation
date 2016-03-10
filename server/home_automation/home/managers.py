import home.config as config

import django.utils.timezone
from django.db import models

import datetime


class DeviceManager(models.Manager):
    def count_alive(self):
        return self.filter(last_seen__gte=self.__heartbeat_time_limit).count()

    def count_dead(self):
        return self.filter(last_seen__lt=self.__heartbeat_time_limit).count()

    @property
    def __heartbeat_time_limit(self):
        now = django.utils.timezone.now()
        return now - config.device_heartbeat_timeout
