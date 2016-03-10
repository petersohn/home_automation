import os
import pickle
import socketserver
import stat


class RequestHandler(socketserver.BaseRequestHandler):
    def __init__(self, handler, *args, **kwargs):
        self.handler = handler
        super(RequestHandler, self).__init__(*args, **kwargs)

    def handle(self):
        object = pickle.loads(self.request[0])
        self.handler(object)


def startExecutor(address, handler):
    def createRequestHandler(*args, **kwargs):
        return RequestHandler(handler, *args, **kwargs)
    server = socketserver.UnixDatagramServer(address, createRequestHandler)
    os.chmod(address, stat.S_IRUSR | stat.S_IWUSR)
    server.serve_forever()
