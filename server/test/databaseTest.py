#!/usr/bin/env python2

import database
import test_globals

import datetime
import psycopg2
import unittest
import sys


class DatabaseTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        connection = psycopg2.connect(test_globals.connectString)
        script = open(test_globals.sqlDirectory + "/tables.sql")
        connection.cursor().execute(script.read())
        connection.commit()

    def cleanup(self):
        connection = psycopg2.connect(test_globals.connectString)
        script = open(test_globals.sqlDirectory + "/cleanup.sql")
        connection.cursor().execute(script.read())
        connection.commit()

    def setUp(self):
        self.cleanup()
        self.session = database.Session(test_globals.connectString)

    def tearDown(self):
        self.cleanup()

    def createInputData(self, name, ip):
        return {"device": {"name": name, "ip": ip}}

    def test_log(self):
        severity = 'info'
        message = 'some test message'
        self.session.log(severity, message)

        connection = psycopg2.connect(test_globals.connectString)
        cursor = connection.cursor()
        cursor.execute("select severity, message, device_id, pin_id " +
                "from log")
        result = cursor.fetchall()
        self.assertListEqual(result, [(severity, message, None, None)])

    def test_log_device(self):
        connection = psycopg2.connect(test_globals.connectString)
        cursor = connection.cursor()
        cursor.execute("insert into device (name, ip, last_seen) values " +
                "(%s, %s, %s) returning device_id",
                ("foo", "192.168.1.10", datetime.datetime(2016, 1, 1)))
        deviceId, = cursor.fetchone()
        connection.commit()

        severity = 'warning'
        message = 'some warning message'
        self.session.log(severity, message, device = deviceId)

        cursor.execute("select severity, message, device_id, pin_id from log")
        result = cursor.fetchall()
        self.assertListEqual(result, [(severity, message, deviceId, None)])

    def test_log_device_pin(self):
        connection = psycopg2.connect(test_globals.connectString)
        cursor = connection.cursor()
        cursor.execute("insert into device (name, ip, last_seen) values " +
                "(%s, %s, %s) returning device_id",
                ("foo", "192.168.1.10", datetime.datetime(2016, 1, 1)))
        deviceId, = cursor.fetchone()
        cursor.execute("insert into pin (name, device_id, type) values " +
                "(%s, %s, %s) returning pin_id",
                ("bar", deviceId, "input"))
        pinId, = cursor.fetchone()
        connection.commit()

        severity = 'error'
        message = 'some error message'
        self.session.log(severity, message, device = deviceId, pin = pinId)

        cursor.execute("select severity, message, device_id, pin_id from log")
        result = cursor.fetchall()
        self.assertListEqual(result, [(severity, message, deviceId, pinId)])

    def test_updateDevice_add_new_device(self):
        deviceName = "someDevice"
        deviceIp = "192.168.1.10"
        data = self.createInputData(deviceName, deviceIp)

        begin = datetime.datetime.now()
        self.session.updateDevice(data)
        end = datetime.datetime.now()

        connection = psycopg2.connect(test_globals.connectString)
        cursor = connection.cursor()
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

        connection = psycopg2.connect(test_globals.connectString)
        cursor = connection.cursor()
        cursor.execute("select name, ip from device")
        result = cursor.fetchall()
        self.assertListEqual(result, [(deviceName, deviceIp)])

    def test_updateDevice_add_multiple_devices(self):
        device1Name = "firstDevice"
        device1Ip = "192.168.1.10"
        device1Data = self.createInputData(device1Name, device1Ip)
        device2Name = "secondDevice"
        device2Ip = "192.168.2.23"
        device2Data = self.createInputData(device2Name, device2Ip)

        self.session.updateDevice(device1Data)
        self.session.updateDevice(device2Data)

        connection = psycopg2.connect(test_globals.connectString)
        cursor = connection.cursor()
        cursor.execute("select name, ip from device")
        result = cursor.fetchall()
        self.assertListEqual(result,
                [(device1Name, device1Ip), (device2Name, device2Ip)])

