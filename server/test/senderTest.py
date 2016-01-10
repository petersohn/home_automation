#!/usr/bin/env python2

import sender
import test_globals

import httplib
import pickle
import unittest
import urllib


# TODO use mock instead
class FakeSession:
    def __init__(self, address):
        self.address = address


    def getDeviceIp(self, deviceName):
        return self.address


def getGlobalAddressSession():
    return FakeSession(test_globals.testServerAddress)


class TestException(Exception):
    pass


class BadSession:
    def getDeviceIp(self, deviceName):
        raise TestException("")



class SenderTest(unittest.TestCase):
    pass



class RequestTest(SenderTest):
    def setUp(self):
        self.connections = {}
        pass

    def tearDown(self):
        pass


    def test_successfulRequest(self):
        data = "some test data"
        request = sender.Request("someDevice",
                test_globals.echoUrl + "?" + urllib.quote(data),
                getSession=getGlobalAddressSession)
        result = request.execute(self.connections)
        self.assertEqual(result, data)

    def test_multiple_send_requests_work(self):
        data1 = "some test data"
        request1 = sender.Request("someDevice",
                test_globals.echoUrl + "?" + urllib.quote(data1),
                getSession=getGlobalAddressSession)
        result = request1.execute(self.connections)
        self.assertEqual(result, data1)

        data2 = "more test data"
        request2 = sender.Request("someDevice",
                test_globals.echoUrl + "?" + urllib.quote(data2),
                getSession=getGlobalAddressSession)
        result = request2.execute(self.connections)
        self.assertEqual(result, data2)

    def test_request_for_bad_url_throws(self):
        request = sender.Request("someDevice", "/_test/invalid_url",
                getSession=getGlobalAddressSession)
        self.assertRaises(sender.BadResponse, request.execute, self.connections)

    def test_request_with_bad_device_throws(self):
        request = sender.Request("someDevice", test_globals.echoUrl,
                getSession=BadSession)
        self.assertRaises(TestException, request.execute, self.connections)

    def test_request_is_picklable(self):
        data = "some test data"
        request = sender.Request("someDevice",
                test_globals.echoUrl + "?" + urllib.quote(data),
                getSession=getGlobalAddressSession)
        copiedRequest = pickle.loads(pickle.dumps(request))
        result = copiedRequest.execute(self.connections)
        self.assertEqual(result, data)



class ClearDeviceTest(SenderTest):
    def test_clearDevice_removes_correct_ip(self):
        ip1 = "1.2.3.4"
        value1 = "some value"
        ip2 = "3.4.5.6"
        value2 = "other value"
        connections = {ip1: value1, ip2: value2}

        clearDevice = sender.ClearDevice("someDevice",
                (lambda: FakeSession(ip1)))
        clearDevice.execute(connections)
        self.assertEqual(connections, {ip2: value2})


    def test_clearDevice_ignores_non_existing_devices(self):
        ip1 = "1.2.3.4"
        value1 = "some value"
        ip2 = "3.4.5.6"
        value2 = "other value"
        connections = {ip1: value1, ip2: value2}
        expectedResult = connections

        clearDevice = sender.ClearDevice("someDevice",
                (lambda: FakeSession("4.3.2.1")))
        clearDevice.execute(connections)
        self.assertEqual(connections,  expectedResult)



