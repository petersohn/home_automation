#!/usr/bin/env python3

import sender
import test_globals

import http.client
import pickle
import unittest
import unittest.mock
import urllib


class TestException(Exception):
    pass


class BadSession:
    def getDeviceIp(self, deviceName):
        raise TestException("")



class SenderTest(unittest.TestCase):
    def setUp(self):
        self.getSession = unittest.mock.MagicMock()




class RequestTest(SenderTest):
    def setUp(self):
        super(RequestTest, self).setUp()
        self.connections = {}
        self.getSession().getDeviceIp.return_value = \
                test_globals.testServerAddress

    def tearDown(self):
        for ip, connection in self.connections.items():
            connection.close()


    def test_successfulRequest(self):
        deviceName = "someDevice"
        data = "some test data"
        request = sender.Request(deviceName,
                test_globals.echoUrl + "?" + urllib.parse.quote(data),
                getSession=self.getSession)
        result = request.execute(self.connections)
        self.getSession().getDeviceIp.assert_called_once_with(deviceName)
        self.assertEqual(result, data)

    def test_multiple_send_requests_work(self):
        data1 = "some test data"
        request1 = sender.Request("someDevice",
                test_globals.echoUrl + "?" + urllib.parse.quote(data1),
                getSession=self.getSession)
        result = str(request1.execute(self.connections))
        self.assertEqual(result, data1)

        data2 = "more test data"
        request2 = sender.Request("someDevice",
                test_globals.echoUrl + "?" + urllib.parse.quote(data2),
                getSession=self.getSession)
        result = request2.execute(self.connections)
        self.assertEqual(result, data2)

    def test_request_for_bad_url_throws(self):
        request = sender.Request("someDevice", "/_test/invalid_url",
                getSession=self.getSession)
        self.assertRaises(sender.BadResponse, request.execute, self.connections)

    def test_request_with_bad_device_throws(self):
        request = sender.Request("someDevice", test_globals.echoUrl,
                getSession=BadSession)
        self.assertRaises(TestException, request.execute, self.connections)



class ClearDeviceTest(SenderTest):
    def test_clearDevice_removes_correct_ip(self):
        deviceName = "someDevice"
        ip1 = "1.2.3.4"
        value1 = "some value"
        ip2 = "3.4.5.6"
        value2 = "other value"
        connections = {ip1: value1, ip2: value2}

        self.getSession().getDeviceIp.return_value = ip1
        clearDevice = sender.ClearDevice(deviceName, getSession=self.getSession)
        clearDevice.execute(connections)
        self.getSession().getDeviceIp.assert_called_once_with(deviceName)
        self.assertEqual(connections, {ip2: value2})


    def test_clearDevice_ignores_non_existing_devices(self):
        deviceName = "someDevice"
        ip1 = "1.2.3.4"
        value1 = "some value"
        ip2 = "3.4.5.6"
        value2 = "other value"
        connections = {ip1: value1, ip2: value2}
        expectedResult = connections

        self.getSession().getDeviceIp.return_value = "4.3.2.1"
        clearDevice = sender.ClearDevice(deviceName, getSession=self.getSession)
        clearDevice.execute(connections)
        self.getSession().getDeviceIp.assert_called_once_with(deviceName)
        self.assertEqual(connections,  expectedResult)



