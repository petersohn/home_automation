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


    def makeSuccessfulRequest(self, deviceName, deviceIp, devicePort, data):
        self.getSession().getDeviceAddress.return_value = (deviceIp, devicePort)
        request = sender.Request(deviceName, data, getSession=self.getSession,
                httpConnection = self.httpConnection)
        self.createExpectedResult(data)
        result = request.execute(self.connections)
        self.getSession().getDeviceAddress.assert_called_once_with(deviceName)
        self.connection.request.assert_called_once_with("GET", data,
                headers = ANY)
        self.connection.getresponse.assert_called_once_with()
        self.assertEqual(result, data)

        self.getSession().getDeviceAddress.reset_mock()


    def resetConnectionMocks(self):
        self.httpConnection.reset_mock()
        self.connection.request.reset_mock()
        self.connection.getresponse.reset_mock()


    def test_successful_request(self):
        deviceIp = "1.2.3.4"
        devicePort = 80
        self.makeSuccessfulRequest("someDevice", deviceIp, devicePort,
                "some test data")
        self.httpConnection.assert_called_once_with(deviceIp, port = devicePort,
                timeout = ANY)

    def test_multiple_send_requests_create_only_one_connection_per_device(self):
        ip1 = "1.2.3.4"
        port1 = 81
        ip2 = "7.2.2.5"
        port2 = 82
        self.makeSuccessfulRequest("device1", ip1, port1, "some test data")
        self.httpConnection.assert_called_once_with(ip1, port = port1,
                timeout = ANY)
        self.resetConnectionMocks()

        self.makeSuccessfulRequest("device2", ip2, port2, "other test data")
        self.httpConnection.assert_called_once_with(ip2, port = port2,
                timeout = ANY)
        self.resetConnectionMocks()

        self.makeSuccessfulRequest("device2", ip2, port2, "more data")
        self.httpConnection.assert_not_called()
        self.resetConnectionMocks()

        self.makeSuccessfulRequest("device1", ip1, port1, "asdasdasdasdasd")
        self.httpConnection.assert_not_called()


    def test_request_for_bad_url_throws(self):
        data = "invalid URL"
        deviceIp = "1.2.3.4"
        devicePort = 80
        self.getSession().getDeviceAddress.return_value = (deviceIp, devicePort)
        request = sender.Request("someDevice", data, getSession=self.getSession,
                httpConnection = self.httpConnection)
        self.createExpectedResult(data, 404, "Not Found")
        self.assertRaises(sender.BadResponse, request.execute, self.connections)
        self.httpConnection.assert_called_once_with(deviceIp, port = devicePort,
                timeout = ANY)
        self.connection.request.assert_called_once_with("GET", data,
                headers = ANY)
        self.connection.getresponse.assert_called_once_with()

    def test_request_with_bad_device_does_not_attempt_to_create_http_connection(self):
        data = "some test data"
        self.getSession().getDeviceAddress.side_effect = TestException
        request = sender.Request("someDevice", data,
                getSession=self.getSession,
                httpConnection=self.httpConnection)
        self.assertRaises(TestException, request.execute, self.connections)
        self.httpConnection.assert_not_called()



class ClearDeviceTest(SenderTest):
    def test_clearDevice_removes_correct_ip(self):
        deviceName = "someDevice"
        ip1 = "1.2.3.4"
        port1 = 80
        value1 = "some value"
        ip2 = "3.4.5.6"
        port2 = 81
        value2 = "other value"
        connections = {(ip1, port1): value1, (ip2, port2): value2}

        self.getSession().getDeviceAddress.return_value = (ip1, port1)
        clearDevice = sender.ClearDevice(deviceName, getSession=self.getSession)
        clearDevice.execute(connections)
        self.getSession().getDeviceAddress.assert_called_once_with(deviceName)
        self.assertEqual(connections, {(ip2, port2): value2})


    def test_clearDevice_ignores_non_existing_devices(self):
        deviceName = "someDevice"
        ip1 = "1.2.3.4"
        port1 = 81
        value1 = "some value"
        ip2 = "3.4.5.6"
        port2 = 8080
        value2 = "other value"
        connections = {(ip1, port1): value1, (ip2, port2): value2}
        expectedResult = connections

        self.getSession().getDeviceAddress.return_value = ("4.3.2.1", 80)
        clearDevice = sender.ClearDevice(deviceName, getSession=self.getSession)
        clearDevice.execute(connections)
        self.getSession().getDeviceAddress.assert_called_once_with(deviceName)
        self.assertEqual(connections,  expectedResult)



