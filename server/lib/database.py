import globals
import config.database_config

import datetime
import psycopg2
import sys

class Session:
    def __init__(self, connectString):
        self.connectString = connectString
        self._connect()

    def log(self, severity, description, device = None, pin = None):
        try:
            self._connectIfNeeded()
            self._log(severity, description, device, pin)
            self.connection.commit()
        except:
            if not self.connection.closed:
                self.connection.rollback()

    def updateDevice(self, name, login = False):
        return self._exectueTransactionally(self._updateDevice, name, login)

    def _exectueTransactionally(self, function, *args, **kwargs):
        try:
            result = function(*args, **kwargs)
            self.connection.commit()
            return result
        except:
            if not self.connection.closed:
                self.connection.rollback()
            raise

    def _connect(self):
        self.connection = psycopg2.connect(self.connectString)

    def _connectIfNeeded(self):
        if self.connection.closed:
            self._connect()

    def _updateDevice(self, name, login):
        cursor = self.connection.cursor()
        cursor.execute("select device_id, last_seen from device where " +
                "name = %s", (name,))
        found = cursor.fetchone()
        if found == None:
            cursor.execute("insert into device (name, last_seen) values " +
                    "(%s, %s) returning device_id",
                    (name, datetime.datetime.now()))
            deviceId, = cursor.fetchone()
            self._log("info", "Found new device.", device = deviceId)
            return deviceId
        else:
            deviceId, lastSeen = found
            now = datetime.datetime.now()
            cursor.execute("update device set last_seen = %s where " +
                    "device_id = %s", (now, deviceId))
            if now - lastSeen > globals.deviceHeartbeatTimeout:
                self._log("info", "Lost device reappeared.", device = deviceId)
            elif login:
                self._log("warning", "Device restarted.", device = deviceId)

    def _log(self, severity, description, device = None, pin = None):
        cursor = self.connection.cursor()
        cursor.execute("insert into log (severity, time, message, device_id, " +
                "pin_id) values (%s, %s, %s, %s, %s)",
                (severity, datetime.datetime.now(), description, device, pin))

session = None

def getSession():
    global session
    if session == None:
        session = Session(config.database_config.psql_connect_string)
    return session
