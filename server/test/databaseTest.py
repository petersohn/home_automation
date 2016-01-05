#!/usr/bin/env python2

import database
import globals

import datetime
import psycopg2
import unittest
import sys


class DatabaseTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        connection = psycopg2.connect(globals.connectString)
        script = open(globals.sqlDirectory + "/tables.sql")
        connection.cursor().execute(script.read())
        connection.commit()

    def cleanup(self):
        connection = psycopg2.connect(globals.connectString)
        script = open(globals.sqlDirectory + "/cleanup.sql")
        connection.cursor().execute(script.read())
        connection.commit()

    def setUp(self):
        self.cleanup()
        self.session = database.Session(globals.connectString)

    def tearDown(self):
        self.cleanup()

    def test_log(self):
        severity = 'info'
        message = 'some test message'
        self.session.log(severity, message)

        connection = psycopg2.connect(globals.connectString)
        with connection.cursor() as cursor:
            cursor.execute("select severity, message, device_id, pin_id " +
                    "from log")
            result = cursor.fetchall()
            self.assertListEqual(result, [(severity, message, None, None)])

    def test_log_device(self):
        connection = psycopg2.connect(globals.connectString)
        cursor = connection.cursor()
        cursor.execute("insert into device (name, last_seen) values (%s, %s) " +
                "returning device_id", ("foo", datetime.datetime(2016, 1, 1)))
        deviceId, = cursor.fetchone()
        connection.commit()

        severity = 'warning'
        message = 'some warning message'
        self.session.log(severity, message, device = deviceId)

        cursor.execute("select severity, message, device_id, pin_id from log")
        result = cursor.fetchall()
        self.assertListEqual(result, [(severity, message, deviceId, None)])

    def test_log_device_pin(self):
        connection = psycopg2.connect(globals.connectString)
        cursor = connection.cursor()
        cursor.execute("insert into device (name, last_seen) values (%s, %s) " +
                "returning device_id", ("foo", datetime.datetime(2016, 1, 1)))
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


