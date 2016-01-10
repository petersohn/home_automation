import database

import httplib
import sys


class BadResponse(Exception):
    def __init__(self, status, reason):
        self.status = status
        self.reason = reason

    def __str__(self):
        return str(self.status) + " " + self.reason



class Request:
    def __init__(self, deviceName, path, getSession = database.getSession):
        self.deviceName = deviceName
        self.path = path
        self.getSession = getSession


    def send(self):
        session = self.getSession()
        deviceIp = session.getDeviceIp(self.deviceName)
        connection = httplib.HTTPConnection(deviceIp)
        connection.request("GET", self.path)
        response = connection.getresponse()
        if response.status < 200 or response.status >= 300:
            raise BadResponse(response.status, response.reason)
        return response.read()


def runProcess(queue):
    session = database.getSession()
    while True:
        request = queue.get()
        try:
            request.send()
        except Exception as e:
            session.log("error", "Error sending request: " + str(e),
                    device=request.deviceName)

