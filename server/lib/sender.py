import database

import httplib


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
        connection = httplib.HTTPConnection(
                session.getDeviceIp(self.deviceName))
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

