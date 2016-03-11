#!/usr/bin/env python3

import ExecutorClient
import ExecutorServer

import argparse
import http.client
import os
import queue
import sys
import threading
import traceback

scriptDirectory = os.path.dirname(os.path.abspath(__file__))
sys.path.append(scriptDirectory + "/../home_automation")
os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'home_automation.settings')

import django
django.setup()

from home import models, services


class BadResponse(Exception):
    def __init__(self, status, reason):
        self.status = status
        self.reason = reason
        self.message = str(self.status) + " " + self.reason

    def __str__(self):
        return self.message


def handleGenericException():
    s = traceback.format_exc()
    sys.stderr.write(s + '\n')


class ConnectionInterrupted(Exception):
    pass


class Connection:

    def __init__(self, host, port, responseHandler, exceptionHandler,
                 httpConnection):
        self.host = host
        self.port = port
        self.responseHandler = responseHandler
        self.exceptionHandler = exceptionHandler
        self.httpConnection = httpConnection
        self.queue = queue.Queue()
        self.thread = None

    def isRunning(self):
        return self.thread is not None and self.thread.is_alive()

    def sendRequest(self, url):
        self._start()
        self.queue.put(lambda connection: self._sendRequest(url, connection))

    def cleanup(self):
        if self.isRunning():
            self.queue.put(self._cleanup)
            if self.thread:
                self.thread.join()

    def _runThread(self):
        try:
            connection = self.httpConnection(self.host, self.port, timeout=10)
            while True:
                action = self.queue.get()
                result = action(connection)
                if result is not None:
                    self.responseHandler(result)
        except ConnectionInterrupted:
            pass
        except Exception as e:
            self.exceptionHandler(e)
        except:
            handleGenericException()

    def _start(self):
        if not self.isRunning():
            self.thread = threading.Thread(target=self._runThread)
            self.thread.start()

    def _sendRequest(self, url, connection):
        retries = 2
        while True:
            try:
                connection.request(
                    "GET", url, headers={"Connection": "keep-alive"})
                response = connection.getresponse()
                if response.status < 200 or response.status >= 300:
                    raise BadResponse(response.status, response.reason)
                return response.read().decode("UTF-8")
            except BadResponse:
                connection.close()
                raise
            except:
                connection.close()
                if retries == 0:
                    raise
                retries -= 1

    def _cleanup(self, connection):
        connection.close()
        raise ConnectionInterrupted


def _getDeviceAddress(device_name):
    device = models.Device.objects.get(name=device_name)
    return (device.ip_address, device.port)


class Request:
    def __init__(self, deviceName, url, getDeviceAddress=_getDeviceAddress,
                 httpConnection=http.client.HTTPConnection,
                 connection=Connection):
        self.deviceName = deviceName
        self.url = url
        self.getDeviceAddress = getDeviceAddress
        self.httpConnection = httpConnection
        self.connection = connection

    def __call__(self, httpConnections, responseHandler, exceptionHandler):
        deviceAddress = self.getDeviceAddress(self.deviceName)

        if deviceAddress not in httpConnections:
            actualConnection = self.connection(
                host=deviceAddress[0], port=deviceAddress[1],
                responseHandler=responseHandler,
                exceptionHandler=exceptionHandler,
                httpConnection=self.httpConnection)
            httpConnections[deviceAddress] = actualConnection
        else:
            actualConnection = httpConnections[deviceAddress]

        actualConnection.sendRequest(self.url)


class ClearDevice:
    def __init__(self, deviceName, getDeviceAddress=_getDeviceAddress):
        self.deviceName = deviceName
        self.getDeviceAddress = getDeviceAddress

    def __call__(self, httpConnections, responseHandler, exceptionHandler):
        deviceAddress = self.getDeviceAddress(self.deviceName)
        if deviceAddress in httpConnections:
            httpConnections.pop(deviceAddress).cleanup()


def handleException(exception):
    services.Logger().log(
        severity=models.Log.Severity.ERROR,
        message=("Error sending request: " + exception.__class__.__name__ +
                 ': ' + str(exception)))


class HandleException:
    def __init__(self, exception):
        self.exception = exception

    def __call__(self, httpConnections, responseHandler, exceptionHandler):
        handleException(self.exception)


class ExceptionHandler:
    def __init__(self, client):
        self.client = client

    def __call__(self, e):
        handleGenericException()
        self.client.send(HandleException(e))


class Handler:
    def __init__(self, client):
        self.client = client
        self.connections = {}

    def __call__(self, request):
        try:
            request(self.connections, responseHandler=lambda x: None,
                    exceptionHandler=ExceptionHandler(self.client))
        except KeyboardInterrupt:
            raise
        except Exception as e:
            handleGenericException()
            handleException(e)
        except:
            handleGenericException()


def cleanup(socket):
    try:
        os.remove(socket)
    except OSError:
        pass


def main():
    parser = argparse.ArgumentParser(
        description="Serve asynchronous requests from web server.")
    parser.add_argument("--socket", default="/tmp/home_automation.socket",
                        help="The socket to listen on.")
    arguments = parser.parse_args()

    client = ExecutorClient.ExecutorClient(arguments.socket)
    handler = Handler(client)
    try:
        ExecutorServer.startExecutor(arguments.socket, handler)
    except KeyboardInterrupt:
        print("Interrupted.")
        cleanup(arguments.socket)
    except:
        cleanup(arguments.socket)
        raise


if __name__ == '__main__':
    main()
