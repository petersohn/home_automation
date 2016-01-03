#!/usr/bin/env python2

import database
import globals

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
            cursor.execute("select severity, message from log")
            result = cursor.fetchall()
            self.assertListEqual(result, [(severity, message)])



