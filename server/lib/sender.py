import database

import http.client
import sys
import traceback


class BadResponse(Exception):
    def __init__(self, status, reason):
        self.status = status
        self.reason = reason
        self.message = str(self.status) + " " + self.reason

    def __str__(self):
        return self.message


class Retry:
    pass


class Request:
    def __init__(self, deviceName, path, getSession = database.getSession,
            httpConnection = http.client.HTTPConnection):
        self.deviceName = deviceName
        self.path = path
        self.getSession = getSession
        self.httpConnection = httpConnection


    def execute(self, httpConnections):
        session = self.getSession()
        deviceIp = session.getDeviceIp(self.deviceName)

        if deviceIp not in httpConnections:
            connection = self.httpConnection(deviceIp, timeout=10)
            httpConnections[deviceIp] = connection
        else:
            connection = httpConnections[deviceIp]

        try:
            connection.request("GET", self.path,
                    headers = {"Connection": "keep-alive"})
            response = connection.getresponse()
            if response.status < 200 or response.status >= 300:
                raise BadResponse(response.status, response.reason)
            return response.read().decode("UTF-8")
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
            session.log("error", "Error sending request: " +
                            e.__class__.__name__ + ': ' + str(e),
                    device=request.deviceName)
            handleGenericException()
        except:
            handleGenericException()



