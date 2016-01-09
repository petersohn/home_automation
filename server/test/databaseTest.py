#!/usr/bin/env python2

import database
import test_globals

import datetime
import psycopg2
import unittest
import sys


class DatabaseTest(unittest.TestCase):
    def cleanup(self):
        script = open(test_globals.sqlDirectory + "/cleanup.sql")
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
                "(name, ip, last_seen) values (%s, %s, %s)",
                ("deviceName", "1.2.3.4", datetime.datetime.now()))

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


    def createInputData(self, name, ip, pins = []):
        device = {"name": name, "ip": ip}
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
        self.assertItemsEqual(result, [(severity, message, None, None)])

    def test_log_device(self):
        cursor = self.connection.cursor()
        cursor.execute("insert into device (name, ip, last_seen) values " +
                "(%s, %s, %s) returning device_id",
                ("foo", "192.168.1.10", datetime.datetime(2016, 1, 1)))
        deviceId, = cursor.fetchone()
        self.connection.commit()

        severity = 'warning'
        message = 'some warning message'
        self.session.log(severity, message, device = deviceId)

        cursor.execute("select severity, message, device_id, pin_id from log")
        result = cursor.fetchall()
        self.assertItemsEqual(result, [(severity, message, deviceId, None)])

    def test_log_device_pin(self):
        cursor = self.connection.cursor()
        cursor.execute("insert into device (name, ip, last_seen) values " +
                "(%s, %s, %s) returning device_id",
                ("foo", "192.168.1.10", datetime.datetime(2016, 1, 1)))
        deviceId, = cursor.fetchone()
        cursor.execute("insert into pin (name, device_id, type) values " +
                "(%s, %s, %s) returning pin_id",
                ("bar", deviceId, "input"))
        pinId, = cursor.fetchone()
        self.connection.commit()

        severity = 'error'
        message = 'some error message'
        self.session.log(severity, message, device = deviceId, pin = pinId)

        cursor.execute("select severity, message, device_id, pin_id from log")
        result = cursor.fetchall()
        self.assertItemsEqual(result, [(severity, message, deviceId, pinId)])


    def test_updateDevice_add_new_device(self):
        deviceName = "someDevice"
        deviceIp = "192.168.1.10"
        data = self.createInputData(deviceName, deviceIp)

        begin = datetime.datetime.now()
        self.session.updateDevice(data)
        end = datetime.datetime.now()

        cursor = self.connection.cursor()
        cursor.execute("select name, ip, last_seen from device")
        name, ip, lastSeen = cursor.fetchone()
        self.assertEqual(name, deviceName)
        self.assertEqual(ip, deviceIp)
        self.assertTrue(lastSeen >= begin)
        self.assertTrue(lastSeen <= end)
        self.assertEqual(cursor.fetchone(), None)

    def test_updateDevice_add_existing_device(self):
        deviceName = "someDevice"
        deviceIp = "192.168.1.10"
        data = self.createInputData(deviceName, deviceIp)

        self.session.updateDevice(data)
        self.session.updateDevice(data)

        cursor = self.connection.cursor()
        cursor.execute("select name, ip from device")
        result = cursor.fetchall()
        self.assertItemsEqual(result, [(deviceName, deviceIp)])

    def test_updateDevice_add_multiple_devices(self):
        device1Name = "firstDevice"
        device1Ip = "192.168.1.10"
        device1Data = self.createInputData(device1Name, device1Ip)
        device2Name = "secondDevice"
        device2Ip = "192.168.2.23"
        device2Data = self.createInputData(device2Name, device2Ip)

        self.session.updateDevice(device1Data)
        self.session.updateDevice(device2Data)

        cursor = self.connection.cursor()
        cursor.execute("select name, ip from device")
        result = cursor.fetchall()
        self.assertItemsEqual(result,
                [(device1Name, device1Ip), (device2Name, device2Ip)])

    def test_updateDevice_set_pins(self):
        deviceName = "someDevice"
        deviceIp = "192.168.1.10"
        inputPinName = "inputPin"
        inputPinType = "input"
        outputPinName = "outputPin"
        outputPinType = "output"
        data = self.createInputData(deviceName, deviceIp,
                [(inputPinName, inputPinType), (outputPinName, outputPinType)])

        self.session.updateDevice(data)

        cursor = self.connection.cursor()
        cursor.execute("select device_id from device")
        deviceId, = cursor.fetchone()
        self.assertEqual(cursor.fetchone(), None)

        cursor.execute("select device_id, name, type from pin")
        result = cursor.fetchall()
        self.assertItemsEqual(result, [
                (deviceId, inputPinName, inputPinType),
                (deviceId, outputPinName, outputPinType)])


    def test_getIntendedState_false_if_there_are_no_control_groups(self):
        pinName = "somePin"
        data = self.createInputData("someDevice", "192.168.1.10",
                [(pinName, "output")])
        self.session.updateDevice(data)

        result = self.session.getIntendedState(pinName)
        self.assertFalse(result)

    def insertControlGroup(self, name, state, pins):
            cursor = self.connection.cursor()
            cursor.execute("insert into control_group (name, state) values " +
                    "(%s, %s) returning control_group_id",
                    (name, state))
            controlGroupId = cursor.fetchone()
            for pin in pins:
                cursor.execute("insert into control_output " +
                "(pin_id, control_group_id) values ((" +
                "select pin_id from pin where name = %s), %s)",
                (pin, controlGroupId))

    def test_getIntendedState_false_if_control_group_value_is_false(self):
        pinName = "somePin"
        data = self.createInputData("someDevice", "192.168.1.10",
                [(pinName, "output")])
        self.session.updateDevice(data)

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", False, [pinName])

        result = self.session.getIntendedState(pinName)
        self.assertEquals(result, False)

    def test_getIntendedState_true_if_control_group_value_is_true(self):
        pinName = "somePin"
        data = self.createInputData("someDevice", "192.168.1.10",
                [(pinName, "output")])
        self.session.updateDevice(data)

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", True, [pinName])

        result = self.session.getIntendedState(pinName)
        self.assertEquals(result, True)



