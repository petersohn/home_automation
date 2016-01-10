#!/usr/bin/env python2
# -*- coding: UTF-8 -*-

import database
import sender

import json
import sys

def run(environ, senderQueue, response):
    response.headers = [('Content-Type', 'text/plain')]
    input = environ["wsgi.input"]
    inputData = json.load(input)
    session = database.getSession()

    session.updateDevice(inputData)

    deviceName = inputData["device"]["name"]
    for pin in inputData["pins"]:
        pinName = pin["name"]
        intendedValue = session.getIntendedState(deviceName, pinName)
        if pin["type"] == "output" and pin["value"] != intendedValue:
            session.log(
                    device = deviceName,
                    pin = pinName,
                    severity = "warning",
                    description = "Wrong value of pin.")
            request = sender.Request(deviceName, "/" + pinName + "/" + (
                    "1" if intendedValue else "0"))
            senderQueue.put(request)

    return ''



