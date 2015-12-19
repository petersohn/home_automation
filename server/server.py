#!/usr/bin/env python
import BaseHTTPServer
import json
import traceback

currentDevices = {}

class Handler(BaseHTTPServer.BaseHTTPRequestHandler):
    def do_POST(self):
        try:
            print "--->", self.path
            print "    ", self.client_address
            if (self.path == "/login"):
                contentLength = self.headers.getheader("Content-Length")
                content = self.rfile.read(int(contentLength))
                data = json.loads(content)
                currentDevices[self.client_address[0]] = data["name"]
            elif (self.path == "/logout"):
                currentDevices.pop(self.client_address[0])
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

