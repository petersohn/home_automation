#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

import actions
import database
import sender

import json
import sys

def run(environ, senderQueue, response):
    response.headers = [('Content-Type', 'text/plain')]
    input = environ["wsgi.input"]
    inputData = json.loads(input.read().decode("UTF-8"))
    session = database.getSession()

    session.updateDevice(inputData)

    deviceName = inputData["device"]["name"]
    inputType = inputData.get("type", None)
    if inputType == "login":
        senderQueue.put(sender.ClearDevice(deviceName))

    pins = inputData["pins"]

    intendedStates = session.getIntendedState(deviceName)
    for pin in pins:
        pinName = pin["name"]
        pinValue = pin["value"]
        if inputType == "event":
            changedDevices = session.processTriggers(deviceName, pinName, pinValue)
            for changedDevice, changedPins in changedDevices.items():
                for changedPin, value in changedPins.items():
                    actions.setPin(senderQueue, changedDevice, changedPin, value)
        elif pin["type"] == "output":
            intendedValue = intendedStates.get(deviceName, {}).\
                    get(pinName, pinValue)
            if pinValue != intendedValue:
                session.log(
                        device = deviceName,
                        pin = pinName,
                        severity = "warning",
                        description = "Wrong value of pin.")

                actions.setPin(senderQueue, deviceName, pinName, intendedValue)

    return ''



