#!/usr/bin/env python3

import database
import test_globals

import datetime
import psycopg2
import unittest
import sys


class DatabaseTest(unittest.TestCase):
    def cleanup(self):
        with  open(test_globals.sqlDirectory + "/cleanup.sql") as script:
            self.connection.cursor().execute(script.read())
            self.connection.commit()

    def setUp(self):
        self.connection = psycopg2.connect(test_globals.connectString)
        self.cleanup()

    def tearDown(self):
        self.cleanup()
        self.connection.close()


class TestException(Exception):
    pass


class ExecuteTransactionallyTest(DatabaseTest):
    def insertData(self, connection):
        cursor = connection.cursor()
        cursor.execute("insert into device " +
                "(name, ip, port, last_seen) values (%s, %s, %s, %s)",
                ("deviceName", "1.2.3.4", 80, datetime.datetime.now()))

    def checkData(self, length):
        cursor = self.connection.cursor()
        cursor.execute("select count(*) from device")
        count, = cursor.fetchone()
        self.assertEqual(count, length)

    def insertAndThrow(self, connection):
        self.insertData(connection)
        raise TestException("")


    def test_successful_transaction_is_committed(self):
        connection = psycopg2.connect(test_globals.connectString)
        database.executeTransactionally(connection, self.insertData, connection)
        connection.close()
        self.checkData(1)

    def test_failed_transaction_is_rolled_back(self):
        connection = psycopg2.connect(test_globals.connectString)
        self.assertRaises(TestException, database.executeTransactionally,
                connection, self.insertAndThrow, connection)
        connection.close()
        self.checkData(0)



class SessionTest(DatabaseTest):
    def setUp(self):
        super(SessionTest, self).setUp()
        self.session = database.Session(test_globals.connectString)


    def tearDown(self):
        self.session.closeConnection()
        super(SessionTest, self).tearDown()


    def createInputData(self, name, ip, port, pins = []):
        device = {"name": name, "ip": ip, "port": port}
        pins = [{"name": name, "type": type} for (name, type) in pins]
        return {"device": device, "pins": pins}


    def test_log(self):
        severity = 'info'
        message = 'some test message'
        self.session.log(severity, message)

        cursor = self.connection.cursor()
        cursor.execute("select severity, message, device_id, pin_id " +
                "from log")
        result = cursor.fetchall()
        self.assertCountEqual(result, [(severity, message, None, None)])


    def _pin(self, name, type, expression = None):
        return {"name": name, "type": type, "expression": expression}


    def addDevice(self, name, ip = "192.168.1.10", port = 80,
            seen = datetime.datetime.now(), pins = []):
        cursor = self.connection.cursor()
        cursor.execute(
                """
                insert into device (name, ip, port, last_seen)
                values (%s, %s, %s, %s) returning device_id
                """, (name, ip, port, seen))
        deviceId, = cursor.fetchone()

        pinIds = []
        for pin in pins:
            pinName = pin["name"]
            pinType = pin["type"]
            pinExpression = pin["expression"]
            cursor.execute(
                    """
                    insert into pin (name, device_id, type, expression)
                    values (%s, %s, %s, %s) returning pin_id
                    """,
                    (pinName, deviceId, pinType, pinExpression))
            pinId, = cursor.fetchone()
            pinIds.append(pinId)

        return (deviceId, pinIds)

    def test_log_device(self):
        deviceId, pinIds = database.executeTransactionally(self.connection,
                self.addDevice, "foo")

        severity = 'warning'
        message = 'some warning message'
        self.session.log(severity, message, device = deviceId)

        cursor = self.connection.cursor()
        cursor.execute("select severity, message, device_id, pin_id from log")
        result = cursor.fetchall()
        self.assertCountEqual(result, [(severity, message, deviceId, None)])

    def test_log_device_pin(self):
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, "foo", pins = [self._pin("bar", "input")])

        severity = 'error'
        message = 'some error message'
        self.session.log(severity, message, device = deviceId, pin = pinId)

        cursor = self.connection.cursor()
        cursor.execute("select severity, message, device_id, pin_id from log")
        result = cursor.fetchall()
        self.assertCountEqual(result, [(severity, message, deviceId, pinId)])

    def test_log_device_with_name(self):
        deviceName = "foo"
        deviceId, pinIds = database.executeTransactionally(self.connection,
                self.addDevice, deviceName)

        severity = 'info'
        message = 'some other message'
        self.session.log(severity, message, device = deviceName)

        cursor = self.connection.cursor()
        cursor.execute("select severity, message, device_id, pin_id from log")
        result = cursor.fetchall()
        self.assertCountEqual(result, [(severity, message, deviceId, None)])

    def test_log_pin_with_name(self):
        pinName = "baar"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, "foo", pins = [self._pin(pinName, "input")])

        severity = 'error'
        message = 'some other error message'
        self.session.log(severity, message, device = deviceId, pin = pinName)

        cursor = self.connection.cursor()
        cursor.execute("select severity, message, device_id, pin_id from log")
        result = cursor.fetchall()
        self.assertCountEqual(result, [(severity, message, deviceId, pinId)])

    def test_log_device_and_pin_with_name(self):
        deviceName = "someDevice"
        pinName = "somePin"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [self._pin(pinName, "input")])

        severity = 'info'
        message = 'yet another message'
        self.session.log(severity, message, device = deviceName, pin = pinName)

        cursor = self.connection.cursor()
        cursor.execute("select severity, message, device_id, pin_id from log")
        result = cursor.fetchall()
        self.assertCountEqual(result, [(severity, message, deviceId, pinId)])


    def test_updateDevice_add_new_device(self):
        deviceName = "someDevice"
        deviceIp = "192.168.1.10"
        devicePort = 80
        data = self.createInputData(deviceName, deviceIp, devicePort)

        begin = datetime.datetime.now()
        self.session.updateDevice(data)
        end = datetime.datetime.now()

        cursor = self.connection.cursor()
        cursor.execute("select name, ip, port, last_seen from device")
        name, ip, port, lastSeen = cursor.fetchone()
        self.assertEqual(name, deviceName)
        self.assertEqual(ip, deviceIp)
        self.assertEqual(port, devicePort)
        self.assertTrue(lastSeen >= begin)
        self.assertTrue(lastSeen <= end)
        self.assertEqual(cursor.fetchone(), None)

    def test_updateDevice_add_existing_device(self):
        deviceName = "someDevice"
        deviceIp = "192.168.1.10"
        devicePort = 81
        data = self.createInputData(deviceName, deviceIp, devicePort)

        self.session.updateDevice(data)
        self.session.updateDevice(data)

        cursor = self.connection.cursor()
        cursor.execute("select name, ip, port from device")
        result = cursor.fetchall()
        self.assertCountEqual(result, [(deviceName, deviceIp, devicePort)])

    def test_updateDevice_add_multiple_devices(self):
        device1Name = "firstDevice"
        device1Ip = "192.168.1.10"
        device1Port = 8080
        device1Data = self.createInputData(device1Name, device1Ip, device1Port)
        device2Name = "secondDevice"
        device2Ip = "192.168.2.23"
        device2Port = 82
        device2Data = self.createInputData(device2Name, device2Ip, device2Port)

        self.session.updateDevice(device1Data)
        self.session.updateDevice(device2Data)

        cursor = self.connection.cursor()
        cursor.execute("select name, ip, port from device")
        result = cursor.fetchall()
        self.assertCountEqual(result, [
                (device1Name, device1Ip, device1Port),
                (device2Name, device2Ip, device2Port)])

    def test_updateDevice_set_pins(self):
        deviceName = "someDevice"
        deviceIp = "192.168.1.10"
        devicePort = 80
        inputPinName = "inputPin"
        inputPinType = "input"
        outputPinName = "outputPin"
        outputPinType = "output"
        data = self.createInputData(deviceName, deviceIp, devicePort,
                [(inputPinName, inputPinType), (outputPinName, outputPinType)])

        self.session.updateDevice(data)

        cursor = self.connection.cursor()
        cursor.execute("select device_id from device")
        deviceId, = cursor.fetchone()
        self.assertEqual(cursor.fetchone(), None)

        cursor.execute("select device_id, name, type from pin")
        result = cursor.fetchall()
        self.assertCountEqual(result, [
                (deviceId, inputPinName, inputPinType),
                (deviceId, outputPinName, outputPinType)])


    def insertParameters(self, parameters):
            cursor = self.connection.cursor()
            for name, value in parameters:
                cursor.execute(
                        """
                        insert into parameter (name, value) values (%s, %s)
                        returning parameter_id
                        """,
                        (name, value))

    def test_getIntendedState_constant_false(self):
        deviceName = "someDevice"
        pinName = "somePin"
        database.executeTransactionally(self.connection,
                self.addDevice, deviceName,
                pins = [self._pin(pinName, "output", "False")])

        result = self.session.getIntendedState(None)
        self.assertEqual(result, {deviceName: {pinName: False}})

    def test_getIntendedState_constant_true(self):
        deviceName = "someDevice"
        pinName = "somePin"
        database.executeTransactionally(self.connection,
                self.addDevice, deviceName,
                pins = [self._pin(pinName, "output", "True")])

        result = self.session.getIntendedState(None)
        self.assertEqual(result, {deviceName: {pinName: True}})

    def test_getIntendedState_depends_on_parameter(self):
        deviceName = "someDevice"
        deviceId, [pin1Id, pin2Id] = database.executeTransactionally(
                self.connection, self.addDevice, deviceName, pins = [
                        self._pin("falsePin", "output",
                                "params.falseParameter"),
                        self._pin("truePin", "output",
                                "params.trueParameter")])

        database.executeTransactionally(self.connection,
                self.insertParameters, [
                        ("trueParameter", 1), ("falseParameter", 0)])

        result = self.session.getIntendedState(None)
        self.assertEqual(result, {deviceName:
                {"falsePin": 0, "truePin": 1}})

    def test_getIntendedState_multiple_devices(self):
        device1Name = "someDevice"
        device2Name = "otherDevice"
        database.executeTransactionally(
                self.connection, self.addDevice, device1Name, pins = [
                        self._pin("falsePin", "output", "0"),
                        self._pin("truePin", "output", "1")])
        database.executeTransactionally(
                self.connection, self.addDevice, device2Name, pins = [
                        self._pin("falsePin2", "output", "0"),
                        self._pin("truePin", "output", "1")])

        result = self.session.getIntendedState(None)
        self.assertEqual(result, {
                device1Name: {"falsePin": 0, "truePin": 1},
                device2Name: {"falsePin2": 0, "truePin": 1}})

    def test_getIntendedState_prints_results_only_for_given_device(self):
        device1Name = "someDevice"
        device2Name = "otherDevice"
        database.executeTransactionally(
                self.connection, self.addDevice, device1Name, pins = [
                        self._pin("falsePin", "output", "0"),
                        self._pin("truePin", "output", "1")])
        database.executeTransactionally(
                self.connection, self.addDevice, device2Name, pins = [
                        self._pin("falsePin2", "output", "0"),
                        self._pin("truePin", "output", "1")])

        result = self.session.getIntendedState(device1Name)
        self.assertEqual(result, {device1Name: {"falsePin": 0, "truePin": 1}})

    def test_getIntendedState_only_print_output_pins(self):
        deviceName = "someDevice"
        pinName = "somePin"
        database.executeTransactionally(self.connection,
                self.addDevice, deviceName,
                pins = [
                        self._pin(pinName, "output", "True"),
                        self._pin("otherPin", "input", "True")])

        result = self.session.getIntendedState(None)
        self.assertEqual(result, {deviceName: {pinName: True}})


    def test_getIntendedState_only_print_pins_with_intended_state(self):
        deviceName = "someDevice"
        pinName = "somePin"
        database.executeTransactionally(self.connection,
                self.addDevice, deviceName,
                pins = [
                        self._pin(pinName, "output", "True"),
                        self._pin("otherPin", "output")])

        result = self.session.getIntendedState(None)
        self.assertEqual(result, {deviceName: {pinName: True}})


    def test_getDeviceAddress_for_one_device(self):
        deviceName = "someDevice"
        deviceIp = "192.168.12.34"
        devicePort = 80
        deviceId, pins = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, ip = deviceIp, port = devicePort)

        self.assertEqual(self.session.getDeviceAddress(deviceName),
                (deviceIp, devicePort))

    def test_getDeviceAddress_for_more_devices(self):
        device1Name = "someDevice"
        device1Ip = "192.168.12.34"
        device1Port = 81
        device1Id, pins = database.executeTransactionally(self.connection,
                self.addDevice, device1Name, ip = device1Ip, port = device1Port)
        device2Name = "otherDevice"
        device2Ip = "10.22.33.44"
        device2Port = 8080
        device2Id, pins = database.executeTransactionally(self.connection,
                self.addDevice, device2Name, ip = device2Ip, port = device2Port)

        self.assertEqual(self.session.getDeviceAddress(device1Name),
                (device1Ip, device1Port))
        self.assertEqual(self.session.getDeviceAddress(device2Name),
                (device2Ip, device2Port))


    def addTrigger(self, pinId, edge, expression):
            cursor = self.connection.cursor()
            cursor.execute("insert into input_trigger " +
                    "(pin_id, edge, expression) values (%s, %s, %s)",
                    (pinId, edge, expression))


    def getParameterValue(self, name):
        cursor = self.connection.cursor()
        cursor.execute("select value from parameter where name = %s",
                (name,))
        result, = cursor.fetchone()
        return result


    def test_processTriggers_set_value_of_parameter(self):
        deviceName = "someDevice"
        pinName = "somePin"
        parameterName = "someParameter"

        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [self._pin(pinName, "input")])
        database.executeTransactionally(self.connection,
                self.insertParameters, [(parameterName, 0)])
        database.executeTransactionally(self.connection, self.addTrigger,
                pinId, "both", "action.setParameter('" + parameterName +
                "', 31)")

        self.session.processTriggers(deviceName, pinName, 1)
        self.assertEqual(self.getParameterValue(parameterName), 31)


    def test_processTriggers_toggle_value_of_parameter(self):
        deviceName = "someDevice"
        pinName = "somePin"
        parameterName = "someParameter"

        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [self._pin(pinName, "input")])
        database.executeTransactionally(self.connection,
                self.insertParameters, [(parameterName, 0)])
        database.executeTransactionally(self.connection, self.addTrigger,
                pinId, "both", "action.toggleParameter('" +
                parameterName + "')")

        self.session.processTriggers(deviceName, pinName, 1)
        self.assertEqual(self.getParameterValue(parameterName), 1)
        self.session.processTriggers(deviceName, pinName, 1)
        self.assertEqual(self.getParameterValue(parameterName), 0)


    def test_processTriggers_trigger_to_the_correct_edge(self):
        deviceName = "someDevice"
        pinName = "somePin"
        riseParameter = "riseParameter"
        fallParameter = "fallParameter"
        bothParameter = "bothParameter"

        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [self._pin(pinName, "input")])
        database.executeTransactionally(self.connection,
                self.insertParameters, [(riseParameter, 0),
                        (fallParameter, 0), (bothParameter, 0)])
        database.executeTransactionally(self.connection, self.addTrigger,
                pinId, "rising", "action.toggleParameter('" +
                riseParameter + "')")
        database.executeTransactionally(self.connection, self.addTrigger,
                pinId, "falling", "action.toggleParameter('" +
                fallParameter + "')")
        database.executeTransactionally(self.connection, self.addTrigger,
                pinId, "both", "action.toggleParameter('" +
                bothParameter + "')")

        self.session.processTriggers(deviceName, pinName, 1)
        self.assertEqual(self.getParameterValue(riseParameter), 1)
        self.assertEqual(self.getParameterValue(fallParameter), 0)
        self.assertEqual(self.getParameterValue(bothParameter), 1)

        self.session.processTriggers(deviceName, pinName, 0)
        self.assertEqual(self.getParameterValue(riseParameter), 1)
        self.assertEqual(self.getParameterValue(fallParameter), 1)
        self.assertEqual(self.getParameterValue(bothParameter), 0)


    def test_processTriggers_return_with_modified_pins(self):
        inputDevice = "firstDevice"
        inputPin = "somePin"
        outputDevice = "secondDevice"
        notChangedPin = "pin1"
        changedPin = "pin2"
        parameterName = "someParameter"

        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, inputDevice, pins = [
                        self._pin(inputPin, "input")])
        database.executeTransactionally(self.connection,
                self.addDevice, outputDevice, pins = [
                        self._pin(changedPin, "output", "params." +
                                parameterName),
                        self._pin(notChangedPin, "output", "False")])
        database.executeTransactionally(self.connection,
                self.insertParameters, [(parameterName, 0)])
        database.executeTransactionally(self.connection, self.addTrigger,
                pinId, "both", "action.setParameter('" + parameterName +
                "', 1)")

        result = self.session.processTriggers(inputDevice, inputPin, 1)
        self.assertEqual(result, {outputDevice: {changedPin: 1}})




