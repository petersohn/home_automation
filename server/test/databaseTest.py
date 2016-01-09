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

    def addDevice(self, name, ip = "192.168.1.10",
            seen = datetime.datetime.now(), pins = []):
        cursor = self.connection.cursor()
        cursor.execute("insert into device (name, ip, last_seen) values " +
                "(%s, %s, %s) returning device_id", (name, ip, seen))
        deviceId, = cursor.fetchone()

        pinIds = []
        for pin in pins:
            pinName, pinType = pin
            cursor.execute("insert into pin (name, device_id, type) values " +
                    "(%s, %s, %s) returning pin_id",
                    (pinName, deviceId, pinType))
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
        self.assertItemsEqual(result, [(severity, message, deviceId, None)])

    def test_log_device_pin(self):
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, "foo", pins = [("bar", "input")])

        severity = 'error'
        message = 'some error message'
        self.session.log(severity, message, device = deviceId, pin = pinId)

        cursor = self.connection.cursor()
        cursor.execute("select severity, message, device_id, pin_id from log")
        result = cursor.fetchall()
        self.assertItemsEqual(result, [(severity, message, deviceId, pinId)])

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
        self.assertItemsEqual(result, [(severity, message, deviceId, None)])

    def test_log_pin_with_name(self):
        pinName = "baar"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, "foo", pins = [(pinName, "input")])

        severity = 'error'
        message = 'some other error message'
        self.session.log(severity, message, device = deviceId, pin = pinName)

        cursor = self.connection.cursor()
        cursor.execute("select severity, message, device_id, pin_id from log")
        result = cursor.fetchall()
        self.assertItemsEqual(result, [(severity, message, deviceId, pinId)])

    def test_log_device_and_pin_with_name(self):
        deviceName = "someDevice"
        pinName = "somePin"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [(pinName, "input")])

        severity = 'info'
        message = 'yet another message'
        self.session.log(severity, message, device = deviceName, pin = pinName)

        cursor = self.connection.cursor()
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
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, "someDevice", pins = [(pinName, "output")])

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
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, "someDevice", pins = [(pinName, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", False, [pinName])

        result = self.session.getIntendedState(pinName)
        self.assertEquals(result, False)

    def test_getIntendedState_true_if_control_group_value_is_true(self):
        pinName = "somePin"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, "someDevice", pins = [(pinName, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", True, [pinName])

        result = self.session.getIntendedState(pinName)
        self.assertEquals(result, True)

    def test_getIntendedState_true_if_one_of_control_group_values_is_true(self):
        pinName = "somePin"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, "someDevice", pins = [(pinName, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", True, [pinName])
        database.executeTransactionally(self.connection,
                self.insertControlGroup, "otherControlGroup", False, [pinName])

        result = self.session.getIntendedState(pinName)
        self.assertEquals(result, True)

    def test_getIntendedState_false_if_none_of_control_group_values_is_true(self):
        pinName = "somePin"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, "someDevice", pins = [(pinName, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", False, [pinName])
        database.executeTransactionally(self.connection,
                self.insertControlGroup, "otherControlGroup", False, [pinName])

        result = self.session.getIntendedState(pinName)
        self.assertEquals(result, False)

    def test_getIntendedState_true_if_all_of_control_group_values_is_true(self):
        pinName = "somePin"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, "someDevice", pins = [(pinName, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", True, [pinName])
        database.executeTransactionally(self.connection,
                self.insertControlGroup, "otherControlGroup", True, [pinName])

        result = self.session.getIntendedState(pinName)
        self.assertEquals(result, True)

    def test_getIntendedState_true_for_the_correct_pin(self):
        truePinName = "somePin"
        falsePinName = "otherPin"
        deviceId, [truePinId, falsePinId] = \
                database.executeTransactionally(self.connection,
                self.addDevice, "someDevice", pins = [
                        (truePinName, "output"), (falsePinName, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", True,
                [truePinName])

        truePinIntendedState = self.session.getIntendedState(truePinName)
        self.assertEquals(truePinIntendedState, True)
        falsePinIntendedState = self.session.getIntendedState(falsePinName)
        self.assertEquals(falsePinIntendedState, False)

    def test_getIntendedState_one_conrol_group_controls_more_pins(self):
        pin1Name = "somePin"
        pin2Name = "otherPin"
        deviceId, [pin1Id, pin2Id] = \
                database.executeTransactionally(self.connection,
                self.addDevice, "someDevice", pins = [
                        (pin1Name, "output"), (pin2Name, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", True,
                [pin1Name, pin2Name])

        pin1IntendedState = self.session.getIntendedState(pin1Name)
        self.assertEquals(pin1IntendedState, True)
        pin2IntendedState = self.session.getIntendedState(pin2Name)
        self.assertEquals(pin2IntendedState, True)




