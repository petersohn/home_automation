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


class Variables:
    def __init__(self, session):
        self.session = session

    def get(self, name):
        return self.session._getVariable(name)

    def set(self, name, value):
        self.session._setVariable(name, value)

    def toggle(self, name, modulo = 2):
        self.session._toggleVariable(name, modulo)


class Devices:
    def __init__(self, session):
        self.session = session

    def isAlive(self, name):
        return self.session._isDeviceAlive(name)

    def countAlive(self):
        return self.session._countAliveDevices()

    def countDead(self):
        return self.session._countDeadDevices()


class Logger:
    def __init__(self, session):
        self.session = session

    def log(self, message):
        self.session._log("info", message, None, None)


class Session:
    def __init__(self, connectString):
        self.connectString = connectString
        self.variables = Variables(self)
        self.devices = Devices(self)
        self.logger = Logger(self)
        self._connect()


    def closeConnection(self):
        if not self.connection.closed:
            self.connection.close()


    def log(self, severity, message, device = None, pin = None):
        try:
            self._connectIfNeeded()
            self._log(severity, message, device, pin)
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
        initialStates = self._getIntendedState(None)
        inputType = data.get("type", None)
        isLogin = "type" in data and inputType == "login"
        deviceData = data["device"]
        deviceName = deviceData["name"]
        ip = deviceData["ip"]
        port = deviceData["port"]
        version = deviceData.get("version", 1)
        deviceId = self._updateDeviceData(deviceName, ip, port, version, isLogin)
        if inputType != "event":
            self._updatePinData(deviceId, data["pins"])
        newStates = self._getIntendedState(None)
        return self._getChangedStates(initialStates, newStates)


    def _updateDeviceData(self, name, ip, port, version, isLogin):
        cursor = self.connection.cursor()
        cursor.execute("select device_id, last_seen from device where " +
                "name = %s", (name,))
        found = cursor.fetchone()
        if found is None:
            cursor.execute(
                    """
                    insert into device (name, ip, port, version, last_seen)
                    values (%s, %s, %s, %s, %s) returning device_id
                    """,
                    (name, ip, port, version, datetime.datetime.now()))
            deviceId, = cursor.fetchone()
            self._log("info", "Found new device.", device = deviceId)
        else:
            deviceId, lastSeen = found
            now = datetime.datetime.now()
            cursor.execute(
                    """
                    update device set ip = %s, port = %s, version = %s,
                            last_seen = %s
                    where device_id = %s
                    """, (ip, port, version, now, deviceId))
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

    def _executeNoReturnQuery(self, query, values):
        cursor = self.connection.cursor()
        cursor.execute(query, values)


    def _executeSingleReturnQuery(self, query, values):
        cursor = self.connection.cursor()
        cursor.execute(query, values)
        return cursor.fetchone()


    def _setVariable(self, name, value):
        self._executeNoReturnQuery(
                "update variable set value = %s where name = %s",
                (value, name))


    def _toggleVariable(self, name, modulo):
        self._executeNoReturnQuery(
                "update variable set value = (value + 1) %% %s where name = %s",
                (modulo, name))


    def _getVariable(self, name):
        return self._executeSingleReturnQuery(
                "select value from variable where name = %s", (name,))[0]


    def _isDeviceAlive(self, name):
        return self._executeSingleReturnQuery(
                "select last_seen >= %s from device where name = %s",
                (self._getTimeLimit(), name))[0]


    def _countAliveDevices(self):
        return self._executeSingleReturnQuery(
                "select count(*) from device where last_seen >= %s",
                (self._getTimeLimit(),))[0]


    def _countDeadDevices(self):
        return self._executeSingleReturnQuery(
                "select count(*) from device where last_seen < %s",
                (self._getTimeLimit(),))[0]


    def _getTimeLimit(self):
        return datetime.datetime.now() - globals.deviceHeartbeatTimeout


    def _getIntendedState(self, deviceName):
        cursor = self.connection.cursor()
        sql = \
                """
                select device.name, pin.name, expression.value
                from device join pin using (device_id)
                        left join expression using (expression_id)
                where expression is not null and type = 'output'
                """
        values = ()
        if deviceName is not None:
            sql += " and device.name = %s"
            values = (deviceName,)
        else:
            sql += " and device.last_seen >= %s"
            values = (self._getTimeLimit(),)
        cursor.execute(sql, values)

        result = {}
        for (deviceName, pinName, expression) in cursor.fetchall():
            result.setdefault(deviceName, {})[pinName] = \
                    eval(expression, {}, {
                            "var": self.variables,
                            "dev": self.devices})

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
                select expression.value, device.name, pin.name
                from device join pin using (device_id)
                        join input_trigger using (pin_id)
                        join expression on (
                                input_trigger.expression_id =
                                expression.expression_id)
                where (input_trigger.edge = 'both' or input_trigger.edge = %s)
                        and device.name = %s and pin.name = %s
                """, (edge, deviceName, pinName))

        for expression, deviceName, pinName, in cursor.fetchall():
            exec(expression, {}, {
                    "pin": self.Pin(deviceName, pinName, pinValue),
                    "var": self.variables,
                    "dev": self.devices,
                    "log": self.logger})

        newStates = self._getIntendedState(None)
        return self._getChangedStates(initialStates, newStates)


    def _getChangedStates(self, initialStates, newStates):
        result = {}
        for deviceName, pinInfo in newStates.items():
            for pinName, value in pinInfo.items():
                if value != initialStates.get(deviceName, {}).get(pinName):
                    result.setdefault(deviceName, {})[pinName] = value

        return result


    def _findValue(self, value, finder):
        if value is None:
            return None
        if type(value) == int:
            return value
        return finder(value)

    def _log(self, severity, message, device = None, pin = None):
        deviceId = self._findValue(device, self.getDeviceId)
        pinId = self._findValue(pin, self.getPinId)

        cursor = self.connection.cursor()
        cursor.execute("insert into log (severity, time, message, device_id, " +
                "pin_id) values (%s, %s, %s, %s, %s)",
                (severity, datetime.datetime.now(), message, deviceId,
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
