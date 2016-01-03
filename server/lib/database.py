import config.database_config

import datetime
import psycopg2
import sys

class Session:
    def __init__(self):
        self._connect()
        sys.stderr.write("---------------- asdasdasd\n")

    def log(self, severity, description):
        self._connectIfNeeded()
        cursor = self.connection.cursor()
        cursor.execute("insert into log (severity, time, message) values " +
                "(%s, %s, %s)", (severity, datetime.datetime.now(), description))
        self.connection.commit()

    def _connect(self):
        self.connection = psycopg2.connect(
                config.database_config.psql_connect_string)

    def _connectIfNeeded(self):
        if self.connection.closed:
            self._connect()

def getSession():
    return Session()
