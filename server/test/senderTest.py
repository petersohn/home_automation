#!/usr/bin/env python2

import sender
import test_globals

import httplib
import pickle
import unittest
import urllib


# TODO use mock instead
class FakeSession:
    def getDeviceIp(self, deviceName):
        return test_globals.testServerAddress


class TestException(Exception):
    pass


class BadSession:
    def getDeviceIp(self, deviceName):
        raise TestException("")



class SenderTest(unittest.TestCase):
    def setUp(self):
        pass

    def tearDown(self):
        pass


    def test_successfulRequest(self):
        data = "some test data"
        request = sender.Request("someDevice",
                test_globals.echoUrl + "?" + urllib.quote(data),
                getSession=FakeSession)
        result = request.send()
        self.assertEqual(result, data)

    def test_request_for_bad_url_throws(self):
        request = sender.Request("someDevice", "/_test/invalid_url",
                getSession=FakeSession)
        self.assertRaises(sender.BadResponse, request.send)

    def test_request_with_bad_device_throws(self):
        request = sender.Request("someDevice", test_globals.echoUrl,
                getSession=BadSession)
        self.assertRaises(TestException, request.send)

    def test_request_is_picklable(self):
        data = "some test data"
        request = sender.Request("someDevice",
                test_globals.echoUrl + "?" + urllib.quote(data),
                getSession=FakeSession)
        copiedRequest = pickle.loads(pickle.dumps(request))
        result = copiedRequest.send()
        self.assertEqual(result, data)




