import config.database_config

import datetime
import psycopg2
import sys

class Session:
    def __init__(self, connectString):
        self.connectString = connectString
        self._connect()

    def log(self, severity, description, device = None, pin = None):
        self._connectIfNeeded()
        cursor = self.connection.cursor()
        cursor.execute("insert into log (severity, time, message, device_id, " +
                "pin_id) values (%s, %s, %s, %s, %s)",
                (severity, datetime.datetime.now(), description, device, pin))
        self.connection.commit()

    def _connect(self):
        self.connection = psycopg2.connect(self.connectString)

    def _connectIfNeeded(self):
        if self.connection.closed:
            self._connect()

def getSession():
    return Session(config.database_config.psql_connect_string)
