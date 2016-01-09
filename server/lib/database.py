import globals
import config.database_config

import datetime
import psycopg2
import sys

def executeTransactionally(connection, function, *args, **kwargs):
    try:
        result = function(*args, **kwargs)
        connection.commit()
        return result
    except:
        if not connection.closed:
            connection.rollback()
        raise



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
            raise


    def updateDevice(self, data):
        return self._executeTransactionally(self._updateDevice, data)


    def getIntendedState(self, pinName):
        return self._executeTransactionally(self._getIntendedState, pinName)


    def _executeTransactionally(self, function, *args, **kwargs):
        return executeTransactionally(self.connection, function,
                *args, **kwargs)


    def _connect(self):
        self.connection = psycopg2.connect(self.connectString)


    def _connectIfNeeded(self):
        if self.connection.closed:
            self._connect()


    def _updateDevice(self, data):
        isLogin = "type" in data and data["type"] == "login"
        deviceData = data["device"]
        deviceName = deviceData["name"]
        ip = deviceData["ip"]
        deviceId = self._updateDeviceData(deviceName, ip, isLogin)
        if deviceId is not None:
            self._updatePinData(deviceId, data["pins"])


    def _updateDeviceData(self, name, ip, isLogin):
        cursor = self.connection.cursor()
        cursor.execute("select device_id, last_seen from device where " +
                "name = %s", (name,))
        found = cursor.fetchone()
        if found == None:
            cursor.execute("insert into device (name, ip, last_seen) values " +
                    "(%s, %s, %s) returning device_id",
                    (name, ip, datetime.datetime.now()))
            deviceId, = cursor.fetchone()
            self._log("info", "Found new device.", device = deviceId)
            return deviceId
        else:
            deviceId, lastSeen = found
            now = datetime.datetime.now()
            cursor.execute("update device set ip = %s, last_seen = %s where " +
                    "device_id = %s", (ip, now, deviceId))
            if now - lastSeen > globals.deviceHeartbeatTimeout:
                self._log("info", "Lost device reappeared.", device = deviceId)
            elif isLogin:
                self._log("warning", "Device restarted.", device = deviceId)
            return None


    def _updatePinData(self, deviceId, pins):
        cursor = self.connection.cursor()
        for pin in pins:
            name = pin["name"]
            type = pin["type"]
            cursor.execute("insert into pin (device_id, name, type) " +
                    "values (%s, %s, %s) returning pin_id",
                    (deviceId, name, type))


    def _getIntendedState(self, pinName):
        cursor = self.connection.cursor()
        cursor.execute(
                "select count(*) from pin, control_group, control_output " +
                "where control_group.control_group_id = " +
                        "control_output.control_group_id " +
                "and pin.pin_id = control_output.pin_id " +
                "and control_group.state = true")
        count, = cursor.fetchone()
        return count != 0

    def _findValue(self, value, finder):
        if value is None:
            return None
        if type(value) == int or type(value) == long:
            return value
        return finder(value)

    def _log(self, severity, description, device = None, pin = None):
        deviceId = self._findValue(device, self.getDeviceId)
        pinId = self._findValue(pin, self.getPinId)

        cursor = self.connection.cursor()
        cursor.execute("insert into log (severity, time, message, device_id, " +
                "pin_id) values (%s, %s, %s, %s, %s)",
                (severity, datetime.datetime.now(), description, deviceId,
                        pinId))


    def getDeviceId(self, deviceName):
        cursor = self.connection.cursor()
        cursor.execute("select device_id from device where name = %s",
                (deviceName,))
        deviceId, = cursor.fetchone()
        return deviceId

    def getPinId(self, pinName):
        cursor = self.connection.cursor()
        cursor.execute("select pin_id from pin where name = %s", (pinName,))
        pinId, = cursor.fetchone()
        return pinId


session = None

def getSession():
    global session
    if session == None:
        session = Session(config.database_config.psql_connect_string)
    return session
