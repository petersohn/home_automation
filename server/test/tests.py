#!/usr/bin/env python3

import senderTest
import test_globals

import psycopg2


def setUpModule():
    connection = psycopg2.connect(test_globals.connectString)
    with open(test_globals.sqlDirectory + "/tables.sql") as script:
        connection.cursor().execute(script.read())
        connection.commit()


# class SessionTest(databaseTest.SessionTest):
    # pass


# class ExecuteTransactionallyTest(databaseTest.ExecuteTransactionallyTest):
    # pass


# class VariablesTest(databaseTest.VariablesTest):
    # pass


# class DevicesTest(databaseTest.DevicesTest):
    # pass


# class LoggerTest(databaseTest.DevicesTest):
    # pass


class RequestTest(senderTest.RequestTest):
    pass


class ClearDeviceTest(senderTest.ClearDeviceTest):
    pass


class ConnectionTest(senderTest.ConnectionTest):
    pass

