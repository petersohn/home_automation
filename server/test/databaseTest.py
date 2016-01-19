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


    def tearDown(self):
        self.session.closeConnection()
        super(SessionTest, self).tearDown()


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
        self.assertCountEqual(result, [(severity, message, None, None)])

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
        self.assertCountEqual(result, [(severity, message, deviceId, None)])

    def test_log_device_pin(self):
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, "foo", pins = [("bar", "input")])

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
                self.addDevice, "foo", pins = [(pinName, "input")])

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
                self.addDevice, deviceName, pins = [(pinName, "input")])

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
        self.assertCountEqual(result, [(deviceName, deviceIp)])

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
        self.assertCountEqual(result,
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
        self.assertCountEqual(result, [
                (deviceId, inputPinName, inputPinType),
                (deviceId, outputPinName, outputPinType)])


    def test_getIntendedState_false_if_there_are_no_control_groups(self):
        deviceName = "someDevice"
        pinName = "somePin"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [(pinName, "output")])

        result = self.session.getIntendedState(deviceName, pinName)
        self.assertFalse(result)

    def insertControlGroup(self, name, state, pins):
            cursor = self.connection.cursor()
            cursor.execute("insert into control_group (name, state) values " +
                    "(%s, %s) returning control_group_id",
                    (name, state))
            controlGroupId = cursor.fetchone()
            for pin in pins:
                cursor.execute("insert into control_output " +
                "(pin_id, control_group_id) values (%s, %s)",
                (pin, controlGroupId))

    def test_getIntendedState_false_if_control_group_value_is_false(self):
        deviceName = "someDevice"
        pinName = "somePin"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [(pinName, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", False, [pinId])

        result = self.session.getIntendedState(deviceName, pinName)
        self.assertEqual(result, False)

    def test_getIntendedState_true_if_control_group_value_is_true(self):
        deviceName = "someDevice"
        pinName = "somePin"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [(pinName, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", True, [pinId])

        result = self.session.getIntendedState(deviceName, pinName)
        self.assertEqual(result, True)

    def test_getIntendedState_true_if_one_of_control_group_values_is_true(self):
        deviceName = "someDevice"
        pinName = "somePin"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [(pinName, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", True, [pinId])
        database.executeTransactionally(self.connection,
                self.insertControlGroup, "otherControlGroup", False, [pinId])

        result = self.session.getIntendedState(deviceName, pinName)
        self.assertEqual(result, True)

    def test_getIntendedState_false_if_none_of_control_group_values_is_true(self):
        deviceName = "someDevice"
        pinName = "somePin"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [(pinName, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", False, [pinId])
        database.executeTransactionally(self.connection,
                self.insertControlGroup, "otherControlGroup", False, [pinId])

        result = self.session.getIntendedState(deviceName, pinName)
        self.assertEqual(result, False)

    def test_getIntendedState_true_if_all_of_control_group_values_is_true(self):
        deviceName = "someDevice"
        pinName = "somePin"
        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [(pinName, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", True, [pinId])
        database.executeTransactionally(self.connection,
                self.insertControlGroup, "otherControlGroup", True, [pinId])

        result = self.session.getIntendedState(deviceName, pinName)
        self.assertEqual(result, True)

    def test_getIntendedState_true_for_the_correct_pin(self):
        deviceName = "someDevice"
        truePinName = "somePin"
        falsePinName = "otherPin"
        deviceId, [truePinId, falsePinId] = \
                database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [
                        (truePinName, "output"), (falsePinName, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", True,
                [truePinId])

        truePinIntendedState = self.session.getIntendedState(deviceName,
                truePinName)
        self.assertEqual(truePinIntendedState, True)
        falsePinIntendedState = self.session.getIntendedState(deviceName,
                falsePinName)
        self.assertEqual(falsePinIntendedState, False)

    def test_getIntendedState_one_conrol_group_controls_more_pins(self):
        deviceName = "someDevice"
        pin1Name = "somePin"
        pin2Name = "otherPin"
        deviceId, [pin1Id, pin2Id] = \
                database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [
                        (pin1Name, "output"), (pin2Name, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "someControlGroup", True,
                [pin1Id, pin2Id])

        pin1IntendedState = self.session.getIntendedState(deviceName, pin1Name)
        self.assertEqual(pin1IntendedState, True)
        pin2IntendedState = self.session.getIntendedState(deviceName, pin2Name)
        self.assertEqual(pin2IntendedState, True)

    def test_getIntendedState_complex_case_with_more_devices(self):
        pin1Name = "firstPin"
        pin2Name = "secondPin"
        pin3Name = "thirdPin"
        device1Name = "someDevice"
        device1Id, [pin11Id, pin12Id, pin13Id] = \
                database.executeTransactionally(self.connection,
                self.addDevice, device1Name, pins = [
                        (pin1Name, "output"), (pin2Name, "output"),
                        (pin3Name, "output")])
        device2Name = "otherDevice"
        device1Id, [pin21Id, pin22Id, pin23Id] = \
                database.executeTransactionally(self.connection,
                self.addDevice, device2Name, pins = [
                        (pin1Name, "output"), (pin2Name, "output"),
                        (pin3Name, "output")])

        database.executeTransactionally(self.connection,
                self.insertControlGroup, "firstControlGroup", True,
                [pin11Id, pin23Id])
        database.executeTransactionally(self.connection,
                self.insertControlGroup, "secondControlGroup", True,
                [pin11Id])
        database.executeTransactionally(self.connection,
                self.insertControlGroup, "thirdControlGroup", False,
                [pin12Id, pin23Id])
        database.executeTransactionally(self.connection,
                self.insertControlGroup, "fourthControlGroup", False,
                [pin23Id])

        self.assertEqual(self.session.getIntendedState(device1Name, pin1Name),
                True)
        self.assertEqual(self.session.getIntendedState(device1Name, pin2Name),
                False)
        self.assertEqual(self.session.getIntendedState(device1Name, pin3Name),
                False)
        self.assertEqual(self.session.getIntendedState(device2Name, pin1Name),
                False)
        self.assertEqual(self.session.getIntendedState(device2Name, pin2Name),
                False)
        self.assertEqual(self.session.getIntendedState(device2Name, pin3Name),
                True)


    def test_getDeviceIp_for_one_device(self):
        deviceName = "someDevice"
        deviceIp = "192.168.12.34"
        deviceId, pins = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, ip = deviceIp)

        self.assertEqual(self.session.getDeviceIp(deviceName), deviceIp)

    def test_getDeviceIp_for_more_devices(self):
        device1Name = "someDevice"
        device1Ip = "192.168.12.34"
        device1Id, pins = database.executeTransactionally(self.connection,
                self.addDevice, device1Name, ip = device1Ip)
        device2Name = "otherDevice"
        device2Ip = "10.22.33.44"
        device2Id, pins = database.executeTransactionally(self.connection,
                self.addDevice, device2Name, ip = device2Ip)

        self.assertEqual(self.session.getDeviceIp(device1Name), device1Ip)
        self.assertEqual(self.session.getDeviceIp(device2Name), device2Ip)


    def addTrigger(self, pinId, edge, expression):
            cursor = self.connection.cursor()
            cursor.execute("insert into input_trigger " +
                    "(pin_id, edge, expression) values (%s, %s, %s)",
                    (pinId, edge, expression))


    def test_getTriggers_true_finds_rising_and_both_triggers(self):
        deviceName = "someDevice"
        pinName = "somePin"
        risingTrigger = "rising trigger"
        fallingTrigger = "falling trigger"
        bothTrigger = "both trigger"

        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [(pinName, "input")])

        database.executeTransactionally(self.connection, self.addTrigger,
                pinId, "rising", risingTrigger)
        database.executeTransactionally(self.connection, self.addTrigger,
                pinId, "falling", fallingTrigger)
        database.executeTransactionally(self.connection, self.addTrigger,
                pinId, "both", bothTrigger)

        result = self.session.getTriggers(deviceName, pinName, True)
        self.assertCountEqual(result, [risingTrigger, bothTrigger])

    def test_getTriggers_false_finds_falling_and_both_triggers(self):
        deviceName = "someDevice"
        pinName = "somePin"
        risingTrigger = "rising trigger"
        fallingTrigger = "falling trigger"
        bothTrigger = "both trigger"

        deviceId, [pinId] = database.executeTransactionally(self.connection,
                self.addDevice, deviceName, pins = [(pinName, "input")])

        database.executeTransactionally(self.connection, self.addTrigger,
                pinId, "rising", risingTrigger)
        database.executeTransactionally(self.connection, self.addTrigger,
                pinId, "falling", fallingTrigger)
        database.executeTransactionally(self.connection, self.addTrigger,
                pinId, "both", bothTrigger)

        result = self.session.getTriggers(deviceName, pinName, False)
        self.assertCountEqual(result, [fallingTrigger, bothTrigger])

    def test_getTriggers_finds_triggers_for_correct_pin(self):
        pin1Name = "firstPin"
        pin2Name = "secondPin"
        pin3Name = "thirdPin"
        device1Name = "someDevice"
        device1Id, [pin11Id, pin12Id, pin13Id] = \
                database.executeTransactionally(self.connection,
                self.addDevice, device1Name, pins = [
                        (pin1Name, "output"), (pin2Name, "output"),
                        (pin3Name, "output")])
        device2Name = "otherDevice"
        device1Id, [pin21Id, pin22Id, pin23Id] = \
                database.executeTransactionally(self.connection,
                self.addDevice, device2Name, pins = [
                        (pin1Name, "output"), (pin2Name, "output"),
                        (pin3Name, "output")])

        database.executeTransactionally(self.connection, self.addTrigger,
                pin11Id, "both", "trigger11")
        database.executeTransactionally(self.connection, self.addTrigger,
                pin12Id, "both", "trigger12")
        database.executeTransactionally(self.connection, self.addTrigger,
                pin13Id, "both", "trigger13")
        database.executeTransactionally(self.connection, self.addTrigger,
                pin21Id, "both", "trigger21")
        database.executeTransactionally(self.connection, self.addTrigger,
                pin22Id, "both", "trigger22")
        database.executeTransactionally(self.connection, self.addTrigger,
                pin23Id, "both", "trigger23")

        result = self.session.getTriggers(device1Name, pin3Name, False)
        self.assertEqual(result, ["trigger13"])
        result = self.session.getTriggers(device2Name, pin2Name, True)
        self.assertEqual(result, ["trigger22"])


    def getControlGroupState(self, name):
        cursor = self.connection.cursor()
        cursor.execute("select state from control_group where name = %s",
                (name,))
        result, = cursor.fetchone()
        return result


    def test_setControlGroup_set_state_of_control_group(self):
        controlGroupName = "someControlGroup"
        database.executeTransactionally(self.connection,
                self.insertControlGroup, controlGroupName, False, [])

        self.session.setControlGroup(controlGroupName, False)
        self.assertEqual(self.getControlGroupState(controlGroupName), False)
        self.session.setControlGroup(controlGroupName, True)
        self.assertEqual(self.getControlGroupState(controlGroupName), True)
        self.session.setControlGroup(controlGroupName, True)
        self.assertEqual(self.getControlGroupState(controlGroupName), True)
        self.session.setControlGroup(controlGroupName, False)
        self.assertEqual(self.getControlGroupState(controlGroupName), False)


    def test_setControlGroup_set_state_of_the_correct_control_group(self):
        goodControlGroupName = "goodControlGroup"
        badControlGroupName = "badControlGroup"
        database.executeTransactionally(self.connection,
                self.insertControlGroup, goodControlGroupName, False, [])
        database.executeTransactionally(self.connection,
                self.insertControlGroup, badControlGroupName, False, [])

        self.session.setControlGroup(goodControlGroupName, True)
        self.assertEqual(self.getControlGroupState(goodControlGroupName), True)
        self.assertEqual(self.getControlGroupState(badControlGroupName), False)


    def test_toggleControlGroup_toggles_control_group_state(self):
        controlGroupName = "someControlGroup"
        database.executeTransactionally(self.connection,
                self.insertControlGroup, controlGroupName, False, [])

        self.session.toggleControlGroup(controlGroupName)
        self.assertEqual(self.getControlGroupState(controlGroupName), True)
        self.session.toggleControlGroup(controlGroupName)
        self.assertEqual(self.getControlGroupState(controlGroupName), False)
        self.session.toggleControlGroup(controlGroupName)
        self.assertEqual(self.getControlGroupState(controlGroupName), True)


    def test_toggleControlGroup_toggles_correct_control_Group(self):
        goodControlGroupName = "goodControlGroup"
        badControlGroupName = "badControlGroup"
        database.executeTransactionally(self.connection,
                self.insertControlGroup, goodControlGroupName, False, [])
        database.executeTransactionally(self.connection,
                self.insertControlGroup, badControlGroupName, False, [])

        self.session.toggleControlGroup(goodControlGroupName)
        self.assertEqual(self.getControlGroupState(goodControlGroupName), True)
        self.assertEqual(self.getControlGroupState(badControlGroupName), False)


