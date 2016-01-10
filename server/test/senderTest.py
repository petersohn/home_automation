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
        self.connections = {}
        pass

    def tearDown(self):
        pass


    def test_successfulRequest(self):
        data = "some test data"
        request = sender.Request("someDevice",
                test_globals.echoUrl + "?" + urllib.quote(data),
                getSession=FakeSession)
        result = request.send(self.connections)
        self.assertEqual(result, data)

    def test_multiple_send_requests_work(self):
        data1 = "some test data"
        request1 = sender.Request("someDevice",
                test_globals.echoUrl + "?" + urllib.quote(data1),
                getSession=FakeSession)
        result = request1.send(self.connections)
        self.assertEqual(result, data1)

        data2 = "more test data"
        request2 = sender.Request("someDevice",
                test_globals.echoUrl + "?" + urllib.quote(data2),
                getSession=FakeSession)
        result = request2.send(self.connections)
        self.assertEqual(result, data2)

    def test_request_for_bad_url_throws(self):
        request = sender.Request("someDevice", "/_test/invalid_url",
                getSession=FakeSession)
        self.assertRaises(sender.BadResponse, request.send, self.connections)

    def test_request_with_bad_device_throws(self):
        request = sender.Request("someDevice", test_globals.echoUrl,
                getSession=BadSession)
        self.assertRaises(TestException, request.send, self.connections)

    def test_request_is_picklable(self):
        data = "some test data"
        request = sender.Request("someDevice",
                test_globals.echoUrl + "?" + urllib.quote(data),
                getSession=FakeSession)
        copiedRequest = pickle.loads(pickle.dumps(request))
        result = copiedRequest.send(self.connections)
        self.assertEqual(result, data)




