#!/usr/bin/env python3

import sender

import unittest
import unittest.mock
from unittest.mock import ANY


class TestException(Exception):
    pass



class SenderTest(unittest.TestCase):
    def setUp(self):
        self.getSession = unittest.mock.Mock()




class RequestTest(SenderTest):
    def setUp(self):
        super(RequestTest, self).setUp()
        self.connections = {}
        self.deviceIp = "10.21.32.40"
        self.getSession().getDeviceIp.return_value = self.deviceIp
        self.httpConnection = unittest.mock.Mock()

    def tearDown(self):
        for ip, connection in self.connections.items():
            connection.close()


    def createExpectedResult(self, data, status = 200, reason = "OK"):
        self.connection = self.httpConnection()
        self.connection.getresponse().status = status
        self.connection.getresponse().reason = reason
        self.connection.getresponse().read.return_value = data.encode(
                "UTF-8")
        self.httpConnection.reset_mock()


    def test_successful_request(self):
        deviceName = "someDevice"
        data = "some test data"
        request = sender.Request(deviceName, data, getSession=self.getSession,
                httpConnection = self.httpConnection)
        self.createExpectedResult(data)
        result = request.execute(self.connections)
        self.getSession().getDeviceIp.assert_called_once_with(deviceName)
        self.httpConnection.assert_called_once_with(
                self.deviceIp, timeout = ANY)
        self.connection.request.assert_called_once_with("GET", data,
                headers = ANY)
        self.connection.getresponse.assert_called_once_with()
        self.assertEqual(result, data)

    def test_multiple_send_requests_create_only_one_connection(self):
        data1 = "some test data"
        request1 = sender.Request("someDevice", data1,
                getSession=self.getSession,
                httpConnection=self.httpConnection)
        self.createExpectedResult(data1)
        result = str(request1.execute(self.connections))
        self.httpConnection.assert_called_once_with(
                self.deviceIp, timeout = ANY)
        self.connection.request.assert_called_once_with("GET", data1,
                headers = ANY)
        self.connection.getresponse.assert_called_once_with()
        self.assertEqual(result, data1)

        self.httpConnection.reset_mock()
        self.connection.request.reset_mock()
        self.connection.getresponse.reset_mock()

        data2 = "more test data"
        request2 = sender.Request("someDevice", data2,
                getSession=self.getSession,
                httpConnection = self.httpConnection)
        self.createExpectedResult(data2)
        result = request2.execute(self.connections)
        self.httpConnection.assert_not_called()
        self.connection.request.assert_called_once_with("GET", data2,
                headers = ANY)
        self.connection.getresponse.assert_called_once_with()
        self.assertEqual(result, data2)

    def test_request_for_bad_url_throws(self):
        data = "invalid URL"
        request = sender.Request("someDevice", data, getSession=self.getSession,
                httpConnection = self.httpConnection)
        self.createExpectedResult(data, 404, "Not Found")
        self.assertRaises(sender.BadResponse, request.execute, self.connections)
        self.httpConnection.assert_called_once_with(
                self.deviceIp, timeout = ANY)
        self.connection.request.assert_called_once_with("GET", data,
                headers = ANY)
        self.connection.getresponse.assert_called_once_with()

    def test_request_with_bad_device_does_not_attempt_to_create_http_connection(self):
        data = "some test data"
        self.getSession().getDeviceIp.side_effect = TestException
        request = sender.Request("someDevice", data,
                getSession=self.getSession,
                httpConnection=self.httpConnection)
        self.assertRaises(TestException, request.execute, self.connections)
        self.httpConnection.assert_not_called()



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



