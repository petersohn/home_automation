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


    def send(self, httpConnections):
        session = self.getSession()
        deviceIp = session.getDeviceIp(self.deviceName)

        if deviceIp not in httpConnections:
            connection = httplib.HTTPConnection(deviceIp)
            httpConnections[deviceIp] = connection
        else:
            connection = httpConnections[deviceIp]

        connection.request("GET", self.path,
                headers = {"Connection": "keep-alive"})
        response = connection.getresponse()
        if response.status < 200 or response.status >= 300:
            raise BadResponse(response.status, response.reason)
        return response.read()


def runProcess(queue):
    session = database.getSession()
    connections = {}
    while True:
        request = queue.get()
        try:
            request.send(connections)
        except Exception as e:
            session.log("error", "Error sending request: " + str(e),
                    device=request.deviceName)

