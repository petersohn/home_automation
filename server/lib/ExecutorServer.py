import pickle
import socketserver


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
    server.serve_forever()
