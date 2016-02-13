#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

import datetime
import flipflop
import json
import http.client
import multiprocessing
import os.path
import queue
import RPi.GPIO as GPIO
import sys
import threading
import traceback

lock = threading.Lock()
params = None
pins = []


def handleGenericException():
    s = traceback.format_exc()
    sys.stderr.write(s + '\n')


class Pin:
    def __init__(self, name, output, number):
        self.name = name
        self.output = output
        self.number = number


def getDeviceInfo():
    global params
    return {"name": params["deviceName"], "port": params["port"]}


def getPinInfo(pin):
    return {"type": "output" if pin.output else "input",
            "name": pin.name,
            "value": GPIO.input(pin.number)}


def getModifiedPinInfo():
    global pins
    global pressThreshold

    result = []
    for pin in pins:
        if pin.output:
            continue
        status = GPIO.input(pin.number)
        if pin.status != status:
            pin.status = status
            result.append(getPinInfo(pin))

    return result


def getFullStatus(type = None):
    global pins

    pinData = []
    for pin in pins:
        pinData.append(getPinInfo(pin))
    result = {"device": getDeviceInfo(), "pins": pinData}
    if type is not None:
        result["type"] = type

    return result


def getModifiedPinsContent(type):
    modifiedPins = getModifiedPinInfo()
    if len(modifiedPins) != 0:
        return {"device": getDeviceInfo(), "pins": modifiedPins,
                "type": type}
    else:
        return None


def getContent(path):
    global pins

    tokens = path.split("/")
    numTokens = len(tokens)

    if numTokens == 0 or len(tokens[0]) != 0:
        return None

    if numTokens == 2:
        return getFullStatus()

    pinName = tokens[1]
    try:
        [pin] = [pin for pin in pins if pin.name == pinName]
    except:
        handleGenericException()
        return None

    if numTokens > 2:
        if not pin.output:
            return None

        try:
            value = int(tokens[2])
            GPIO.output(pin.number, value)
        except:
            handleGenericException()
            return None

    return getPinInfo(pin)


def initPins():
    global params
    global pins

    pins = [Pin(pin["name"], pin["output"], pin["number"])
            for pin in params["pins"]]


def sendData(data):
    global params
    server = params["server"]

    try:
        connection = http.client.HTTPConnection(server["address"],
                port = server["port"], timeout = 10)
        connection.request("POST", "/device/status/",
                body = json.dumps(data).encode("UTF-8"),
                headers = {"Content-Type": "application/json"})
        response = connection.getresponse()
    except:
        handleGenericException()
        return False

    if response.status >= 200 and response.status < 300:
        return True
    else:
        sys.stderr.write("<--- " + str(response.status) + " " + 
                response.reason + "\n")
        return False


def sendAndGetNewTimeout(data):
    return 60 if sendData(data) else 5


def runSenderThread(eventQueue):
    global params

    with lock:
        timeout = sendAndGetNewTimeout(getFullStatus("login"))

    while True:
        try:
            object = eventQueue.get(timeout = timeout)
            timeout = sendAndGetNewTimeout(getModifiedPinsContent("event"))
        except queue.Empty:
            timeout = sendAndGetNewTimeout(getFullStatus("heartbeat"))


def getContentSafe(path):
    with lock:
        data = getContent(path)
        return data


def handleRequest(environ, start_response):
    path = environ["REQUEST_URI"]
    data = getContentSafe(path)
    if data is None:
        start_response("404 Not Found", [("Content-Type", "text/plain")])
        yield "Invalid path: " + path
    else:
        start_response("200 OK", [("Content-Type", "application/json")])
        yield json.dumps(data)


def handlePinChanged(eventQueue):
    eventQueue.put(0)


def setupPins(eventQueue):
    global pins
    for pin in pins:
        GPIO.setup(pin.number, GPIO.OUT if pin.output else GPIO.IN)
        pin.status = GPIO.input(pin.number)
        if not pin.output:
            GPIO.add_event_detect(pin.number, GPIO.BOTH,
                    callback = (lambda pin: handlePinChanged(eventQueue)),
                    bouncetime = 100)


if __name__ == "__main__":
    scriptDirectory = os.path.dirname(os.path.abspath(__file__))

    with open(scriptDirectory + "/config.json") as file:
        params = json.load(file)
    initPins()

    try:
        eventQueue = queue.Queue()
        senderThread = threading.Thread(target=runSenderThread,
                args=(eventQueue,), daemon=True)
        GPIO.setmode(GPIO.BOARD)
        setupPins(eventQueue)
        senderThread.start()
        server = flipflop.WSGIServer(handleRequest)
        server.run()
    finally:
        GPIO.cleanup()


