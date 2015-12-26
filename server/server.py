#!/usr/bin/env python
import BaseHTTPServer
import httplib
import json
import signal
import thread
import traceback

class DeviceInfo:
    ledStatus = False

    def __init__(self, name):
        self.name = name;


currentDevices = {}

class Handler(BaseHTTPServer.BaseHTTPRequestHandler):
    def toggleLed(self):
        connection = httplib.HTTPConnection(self.client_address[0], 80)
        deviceInfo = currentDevices[self.client_address[0]]
        deviceInfo.ledStatus = not deviceInfo.ledStatus
        newValue = "1" if deviceInfo.ledStatus else "0"
        print "Led -> ", newValue
        connection.request("GET", "/led/" + newValue)
        response = connection.getresponse()
        print response.status, response.reason
        connection.close()


    def do_POST(self):
        try:
            print "--->", self.path
            print "    ", self.client_address
            if (self.path == "/login"):
                contentLength = self.headers.getheader("Content-Length")
                content = self.rfile.read(int(contentLength))
                data = json.loads(content)
                currentDevices[self.client_address[0]] = DeviceInfo(data["name"])
            elif (self.path == "/logout"):
                currentDevices.pop(self.client_address[0])
            elif (self.path == "/status"):
                contentLength = self.headers.getheader("Content-Length")
                content = self.rfile.read(int(contentLength))
                data = json.loads(content)
                print "Device", currentDevices[self.client_address[0]].name;
                for pin in data["pins"]:
                    print "   ", pin["name"], ",", pin["value"]
                if pin["name"] == "button" and pin["value"] == 0:
                    thread.start_new_thread(Handler.toggleLed, (self,))
            else:
                self.send_error(404)
                return
            self.send_response(204)
            self.send_header("Connection", "close")
            self.end_headers()
        except Exception as e:
            self.send_error(500, str(e));
            traceback.print_exc()


    def do_GET(self):
        try:
            result = []
            for address, name in currentDevices.iteritems():
                result.append(dict(name=name, address=address))
            resultString = json.dumps(result)
            self.send_response(200)
            self.send_header("Connection", "close")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", len(resultString))
            self.end_headers()
            self.wfile.write(resultString)
        except Exception as e:
            self.send_error(500, str(e));
            traceback.print_exc()


if __name__ == "__main__":
    server = BaseHTTPServer.HTTPServer(('', 8080), Handler)
    print "Start"
    server.serve_forever()

