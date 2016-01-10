#!/usr/bin/env python2

import argparse
import os.path
import sys

scriptDirectory = os.path.dirname(os.path.abspath(__file__))
libDirectory = os.path.abspath(scriptDirectory + '/../lib')
sqlDirectory = os.path.abspath(scriptDirectory + '/../sql')
echoUrl = "/_test/echoQuery.py"

connectString=None
testServerAddress=None

def parseArguments():
    parser = argparse.ArgumentParser(
            description="Run tests.")
    parser.add_argument("--connectString", required=True,
            help='The connect string used for psql connection.')
    parser.add_argument("--testServerAddress", required=True,
            help='The address to the test HTTP server.')
    parser.add_argument("unittest_args", nargs='*',
            help='Arguments passed to the unittest framework.')
    arguments = parser.parse_args()
    global connectString
    connectString = arguments.connectString
    global testServerAddress
    testServerAddress = arguments.testServerAddress

    return arguments.unittest_args

