import globals
import config.database_config

import datetime
import copy
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


class Actions:
    def __init__(self, session):
        self.session = session


    def setParameter(self, name, value):
        self.session._setParameter(name, value)


    def toggleParameter(self, name):
        self.session._toggleParameter(name)


class Session:
    def __init__(self, connectString):
        self.connectString = connectString
        self.actions = Actions(self)
        self._connect()


    def closeConnection(self):
        if not self.connection.closed:
            self.connection.close()


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
        self._connectIfNeeded()
        return self._executeTransactionally(self._updateDevice, data)


    def getIntendedState(self, deviceName):
        self._connectIfNeeded()
        return self._executeTransactionally(self._getIntendedState, deviceName)


    def processTriggers(self, deviceName, pinName, pinValue):
        self._connectIfNeeded()
        return self._executeTransactionally(
                self._processTriggers, deviceName, pinName, pinValue)


    def getDeviceAddress(self, deviceName):
        self._connectIfNeeded()
        return self._executeTransactionally(self._getDeviceAddress, deviceName)


    def _executeTransactionally(self, function, *args, **kwargs):
        return executeTransactionally(self.connection, function,
                *args, **kwargs)


    def _connect(self):
        self.connection = psycopg2.connect(self.connectString)


    def _connectIfNeeded(self):
        if self.connection.closed:
            self._connect()


    def _getDeviceAddress(self, deviceName):
        cursor = self.connection.cursor()
        cursor.execute("select ip, port from device where name = %s",
                (deviceName,))
        return cursor.fetchone()


    def _updateDevice(self, data):
        inputType = data.get("type", None)
        isLogin = "type" in data and inputType == "login"
        deviceData = data["device"]
        deviceName = deviceData["name"]
        ip = deviceData["ip"]
        port = deviceData["port"]
        deviceId = self._updateDeviceData(deviceName, ip, port, isLogin)
        if inputType != "event":
            self._updatePinData(deviceId, data["pins"])


    def _updateDeviceData(self, name, ip, port, isLogin):
        cursor = self.connection.cursor()
        cursor.execute("select device_id, last_seen from device where " +
                "name = %s", (name,))
        found = cursor.fetchone()
        if found is None:
            cursor.execute(
                    """
                    insert into device (name, ip, port, last_seen)
                    values (%s, %s, %s, %s) returning device_id
                    """,
                    (name, ip, port, datetime.datetime.now()))
            deviceId, = cursor.fetchone()
            self._log("info", "Found new device.", device = deviceId)
        else:
            deviceId, lastSeen = found
            now = datetime.datetime.now()
            cursor.execute(
                    """
                    update device set ip = %s, port = %s, last_seen = %s
                    where device_id = %s
                    """, (ip, port, now, deviceId))
            if now - lastSeen > globals.deviceHeartbeatTimeout:
                self._log("info", "Lost device reappeared.", device = deviceId)
            elif isLogin:
                self._log("warning", "Device restarted.", device = deviceId)
        return deviceId


    def _updatePinData(self, deviceId, pins):
        cursor = self.connection.cursor()
        names = []
        for pin in pins:
            name = pin["name"]
            type = pin["type"]
            names.append(name)
            cursor.execute(
                    """
                    select pin_id from pin
                    where device_id = %s and name = %s
                    """, (deviceId, name))
            found = cursor.fetchone()

            if found is None:
                cursor.execute(
                        """
                        insert into pin (device_id, name, type)
                        values (%s, %s, %s)
                        """, (deviceId, name, type))
            else:
                cursor.execute( "update pin set type = %s where pin_id = %s",
                        (type, found[0]))

        cursor.execute(
                "delete from pin where device_id = %s and name != ALL(%s)",
                (deviceId, names))


    def _setParameter(self, name, value):
        cursor = self.connection.cursor()
        cursor.execute("update parameter set value = %s where name = %s",
                (value, name))


    def _toggleParameter(self, name):
        cursor = self.connection.cursor()
        cursor.execute("update parameter set value = 1 - value " +
                "where name = %s", (name,))


    def _getIntendedState(self, deviceName):
        parameters = self._getParameterValues()
        cursor = self.connection.cursor()
        sql = \
                """
                select device.name, pin.name, expression
                from device join pin using (device_id)
                where expression is not null and type = 'output'
                """
        values = ()
        if deviceName is not None:
            sql += " and device.name = %s"
            values = (deviceName,)
        cursor.execute(sql, values)

        result = {}
        for (deviceName, pinName, expression) in cursor.fetchall():
            result.setdefault(deviceName, {})[pinName] = \
                    eval(expression, {}, {"params": parameters})

        return result


    class Pin:
        def __init__(self, device, pin, value):
            self.device = device
            self.pin = pin
            self.value = value


    def _processTriggers(self, deviceName, pinName, pinValue):
        initialStates = self._getIntendedState(None)
        edge = "rising" if pinValue else "falling"
        cursor = self.connection.cursor()
        cursor.execute(
                """
                select input_trigger.expression, device.name, pin.name
                from device join pin using (device_id)
                        join input_trigger using (pin_id)
                where (input_trigger.edge = 'both' or input_trigger.edge = %s)
                        and device.name = %s and pin.name = %s
                """, (edge, deviceName, pinName))

        for expression, deviceName, pinName, in cursor.fetchall():
            exec(expression, {}, {
                    "pin": self.Pin(deviceName, pinName, pinValue),
                    "action": self.actions})

        newStates = self._getIntendedState(None)
        result = {}
        for deviceName, pinInfo in newStates.items():
            for pinName, value in pinInfo.items():
                if value != initialStates[deviceName][pinName]:
                    result.setdefault(deviceName, {})[pinName] = value

        return result


    def _getParameterValues(self):
        cursor = self.connection.cursor()
        cursor.execute("select name, value from parameter")
        class Parameter:
            pass

        result = Parameter()
        for name, value in cursor.fetchall():
            setattr(result, name, value)

        return result


    def _findValue(self, value, finder):
        if value is None:
            return None
        if type(value) == int:
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
