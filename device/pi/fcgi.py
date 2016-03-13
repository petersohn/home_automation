#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

import flipflop
import json
import os.path
import queue
import threading

import client
import content
import gpio
import pin


class Main:
    def __init__(self, params):
        self.lock = threading.Lock()
        self.params = params
        self.pins = pin.initPins(params["pins"])
        self.content = content.Content(
            params["deviceName"], params["port"], self.pins)
        self.eventQueue = queue.Queue()

    def sendData(self, data):
        server = self.params["server"]
        return client.sendData(server["host"], server["port"], data)

    def sendAndHandleResult(self, data, type):
        return (60, "heartbeat") if self.sendData(data) else (5, type)

    def runLocked(self, function, *args, **kwargs):
        with self.lock:
            return function(*args, **kwargs)

    def runSenderThread(self):
        type = "login"
        timeout, type = self.sendAndHandleResult(self.runLocked(
                self.content.getFullStatus, type), type)

        while True:
            try:
                self.eventQueue.get(timeout=timeout)
                data = self.runLocked(
                    self.content.getModifiedPinsContent, "event")
                if data is not None:
                    timeout, type = self.sendAndHandleResult(data, type)
            except queue.Empty:
                timeout, type = self.sendAndHandleResult(self.runLocked(
                        self.content.getFullStatus, type), type)

    def handleRequest(self, environ, start_response):
        path = environ["REQUEST_URI"]
        with self.lock:
            data = self.content.getContent(path)

        if data is None:
            start_response("404 Not Found", [("Content-Type", "text/plain")])
            yield "Invalid path: " + path
        else:
            start_response("200 OK", [("Content-Type", "application/json")])
            yield json.dumps(data)

    def handlePinChanged(self):
        self.eventQueue.put(0)

    def run(self):
        with gpio.Context():
            senderThread = threading.Thread(
                target=self.runSenderThread, args=(), daemon=True)
            gpio.setupPins(self.pins, self.handlePinChanged)
            senderThread.start()
            server = flipflop.WSGIServer(self.handleRequest)
            server.run()


if __name__ == "__main__":
    scriptDirectory = os.path.dirname(os.path.abspath(__file__))
    with open(scriptDirectory + "/config.json") as file:
        params = json.load(file)
    Main(params).run()
