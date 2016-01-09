#!/usr/bin/env python2

import databaseTest
import test_globals

import psycopg2


def setUpModule():
    connection = psycopg2.connect(test_globals.connectString)
    script = open(test_globals.sqlDirectory + "/tables.sql")
    connection.cursor().execute(script.read())
    connection.commit()


class SessionTest(databaseTest.SessionTest):
    pass


class ExecuteTransactionallyTest(databaseTest.ExecuteTransactionallyTest):
    pass
