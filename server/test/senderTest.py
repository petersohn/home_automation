#!/usr/bin/env python3

import sender

import unittest
import unittest.mock
from unittest.mock import ANY


class TestException(Exception):
    pass


class SenderTest(unittest.TestCase):
    def setUp(self):
        self.getDeviceAddress = unittest.mock.Mock()
        self.httpConnection = unittest.mock.Mock()
        self.responseHandler = unittest.mock.Mock()
        self.exceptionHandler = unittest.mock.Mock()


class RequestTest(SenderTest):
    def setUp(self):
        super(RequestTest, self).setUp()
        self.connections = {}
        self.connection = unittest.mock.Mock()

    def makeSuccessfulRequest(self, deviceName, deviceIp, devicePort, url):
        self.getDeviceAddress.return_value = (deviceIp, devicePort)
        request = sender.Request(
            deviceName, url, getDeviceAddress=self.getDeviceAddress,
            httpConnection=self.httpConnection, connection=self.connection)
        request(self.connections, responseHandler=self.responseHandler,
                exceptionHandler=self.exceptionHandler)

        self.getDeviceAddress.assert_called_once_with(deviceName)
        self.getDeviceAddress.reset_mock()

    def resetMocks(self):
        self.httpConnection.reset_mock()
        self.connection.reset_mock()
        self.responseHandler.reset_mock()
        self.exceptionHandler.reset_mock()

    def test_successful_request(self):
        deviceIp = "1.2.3.4"
        devicePort = 80
        url = "some/test/url"
        actualConnection = unittest.mock.Mock()
        self.connection.return_value = actualConnection
        self.makeSuccessfulRequest("someDevice", deviceIp, devicePort, url)
        self.connection.assert_called_once_with(
            host=deviceIp, port=devicePort,
            responseHandler=self.responseHandler,
            exceptionHandler=self.exceptionHandler,
            httpConnection=self.httpConnection)
        actualConnection.sendRequest.assert_called_once_with(url)

    def test_multiple_send_requests_create_only_one_connection_per_device(self):
        ip1 = "1.2.3.4"
        port1 = 81
        ip2 = "7.2.2.5"
        port2 = 82
        actualConnection1 = unittest.mock.Mock()
        actualConnection2 = unittest.mock.Mock()
        url1 = "some/test/url"
        url2 = "another/test/url"
        url3 = "something_else"
        url4 = "more_things"

        self.connection.return_value = actualConnection1
        self.makeSuccessfulRequest("device1", ip1, port1, url1)
        self.connection.assert_called_once_with(
            host=ip1, port=port1,
            responseHandler=self.responseHandler,
            exceptionHandler=self.exceptionHandler,
            httpConnection=self.httpConnection)
        actualConnection1.sendRequest.assert_called_once_with(url1)
        actualConnection2.assert_not_called()
        self.resetMocks()
        actualConnection1.reset_mock()
        actualConnection2.reset_mock()

        self.connection.return_value = actualConnection2
        self.makeSuccessfulRequest("device2", ip2, port2, url2)
        self.connection.assert_called_once_with(
            host=ip2, port=port2,
            responseHandler=self.responseHandler,
            exceptionHandler=self.exceptionHandler,
            httpConnection=self.httpConnection)
        actualConnection1.assert_not_called()
        actualConnection2.sendRequest.assert_called_once_with(url2)
        self.resetMocks()
        actualConnection1.reset_mock()
        actualConnection2.reset_mock()

        self.makeSuccessfulRequest("device2", ip2, port2, url3)
        self.connection.assert_not_called()
        actualConnection1.assert_not_called()
        actualConnection2.sendRequest.assert_called_once_with(url3)
        self.resetMocks()
        actualConnection1.reset_mock()
        actualConnection2.reset_mock()

        self.makeSuccessfulRequest("device1", ip1, port1, url4)
        self.connection.assert_not_called()
        actualConnection1.sendRequest.assert_called_once_with(url4)
        actualConnection2.assert_not_called()


class ClearDeviceTest(SenderTest):
    def test_clearDevice_removes_correct_ip(self):
        deviceName = "someDevice"
        ip1 = "1.2.3.4"
        port1 = 80
        connection1 = unittest.mock.Mock()
        ip2 = "3.4.5.6"
        port2 = 81
        connection2 = unittest.mock.Mock()
        connections = {(ip1, port1): connection1, (ip2, port2): connection2}

        self.getDeviceAddress.return_value = (ip1, port1)
        clearDevice = sender.ClearDevice(
            deviceName, getDeviceAddress=self.getDeviceAddress)
        clearDevice(
            connections, responseHandler=self.responseHandler,
            exceptionHandler=self.exceptionHandler)
        self.getDeviceAddress.assert_called_once_with(deviceName)
        self.assertEqual(connections, {(ip2, port2): connection2})
        connection1.cleanup.assert_called_once_with()
        connection2.cleanup.assert_not_called()

    def test_clearDevice_ignores_non_existing_devices(self):
        deviceName = "someDevice"
        ip1 = "1.2.3.4"
        port1 = 81
        connection1 = unittest.mock.Mock()
        ip2 = "3.4.5.6"
        port2 = 8080
        connection2 = unittest.mock.Mock()
        connections = {(ip1, port1): connection1, (ip2, port2): connection2}
        expectedResult = connections

        self.getDeviceAddress.return_value = ("4.3.2.1", 80)
        clearDevice = sender.ClearDevice(
            deviceName, getDeviceAddress=self.getDeviceAddress)
        clearDevice(
            connections, responseHandler=self.responseHandler,
            exceptionHandler=self.exceptionHandler)
        self.getDeviceAddress.assert_called_once_with(deviceName)
        self.assertEqual(connections,  expectedResult)
        connection1.cleanup.assert_not_called()
        connection2.cleanup.assert_not_called()


class ConnectionTest(SenderTest):
    def setUp(self):
        super(ConnectionTest, self).setUp()
        self.host = "1.23.4.5"
        self.port = 8080
        self.connection = sender.Connection(
            self.host, self.port, self.responseHandler, self.exceptionHandler,
            self.httpConnection)

    def test_sendRequest_handles_one_successful_request(self):
        url = "some url"
        result = "some text"

        actualConnection = self.httpConnection()
        actualConnection.getresponse().status = 200
        actualConnection.getresponse().read.return_value = (
            result.encode("UTF-8"))
        actualConnection.getresponse().reset_mock()
        self.httpConnection.reset_mock()

        self.connection.sendRequest(url)
        self.connection.cleanup()

        self.httpConnection.assert_called_once_with(
            self.host, self.port, timeout=ANY)
        actualConnection.request.assert_called_once_with(
            "GET", url, headers=ANY)
        actualConnection.getresponse.assert_called_once_with()
        self.responseHandler.assert_called_once_with(result)
        self.exceptionHandler.assert_not_called()

    def test_sendRequest_handles_one_bad_request(self):
        url = "some url"

        actualConnection = self.httpConnection()
        actualConnection.getresponse().status = 404
        actualConnection.getresponse().reason = "Not Found"
        actualConnection.getresponse().reset_mock()
        self.httpConnection.reset_mock()
        self.exceptionHandler.side_effect = \
            lambda e: self.assertTrue(type(e) is sender.BadResponse)

        self.connection.sendRequest(url)
        self.connection.cleanup()

        self.httpConnection.assert_called_once_with(
            self.host, self.port, timeout=ANY)
        actualConnection.request.assert_called_once_with(
            "GET", url, headers=ANY)
        actualConnection.getresponse.assert_called_once_with()
        self.responseHandler.assert_not_called()
        self.exceptionHandler.assert_called_once_with(ANY)



