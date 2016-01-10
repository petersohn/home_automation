import database

import httplib
import sys
import traceback


class BadResponse(Exception):
    def __init__(self, status, reason):
        self.status = status
        self.reason = reason

    def __str__(self):
        return str(self.status) + " " + self.reason


class Retry:
    pass


class Request:
    def __init__(self, deviceName, path, getSession = database.getSession):
        self.deviceName = deviceName
        self.path = path
        self.getSession = getSession


    def execute(self, httpConnections):
        session = self.getSession()
        deviceIp = session.getDeviceIp(self.deviceName)

        if deviceIp not in httpConnections:
            connection = httplib.HTTPConnection(deviceIp, timeout=10)
            httpConnections[deviceIp] = connection
        else:
            connection = httpConnections[deviceIp]

        try:
            connection.request("GET", self.path,
                    headers = {"Connection": "keep-alive"})
            response = connection.getresponse()
            if response.status < 200 or response.status >= 300:
                raise BadResponse(response.status, response.reason)
            return response.read()
        except:
            connection.close()
            raise


class ClearDevice:
    def __init__(self, deviceName, getSession = database.getSession):
        self.deviceName = deviceName
        self.getSession = getSession

    def execute(self, httpConnections):
        session = self.getSession()
        deviceIp = session.getDeviceIp(self.deviceName)
        httpConnections.pop(deviceIp, None)


def handleGenericException():
    s = traceback.format_exc()
    sys.stderr.write(s + '\n')


def runProcess(queue):
    session = database.getSession()
    connections = {}
    while True:
        request = queue.get()
        try:
            result = request.execute(connections)
            if result.__class__ == Retry:
                queue.put(request)
        except Exception as e:
            session.log("error", "Error sending request: " + str(e),
                    device=request.deviceName)
            handleGenericException()
        except:
            handleGenericException()



