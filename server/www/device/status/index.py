#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

import actions
import database
import sender

import json
import sys

def handleChangedDevices(senderQueue, changedDevices):
    for changedDevice, changedPins in changedDevices.items():
        for changedPin, value in changedPins.items():
            actions.setPin(senderQueue, changedDevice, changedPin, value)

def run(environ, senderQueue, response):
    response.headers = [('Content-Type', 'text/plain')]
    input = environ["wsgi.input"].read().decode("UTF-8")
    sys.stderr.write(input + "\n")
    inputData = json.loads(input)
    remoteAddress = environ["REMOTE_ADDR"]
    inputData["device"].setdefault("ip", remoteAddress)

    session = database.getSession()
    changedDevices = session.updateDevice(inputData)
    handleChangedDevices(senderQueue, changedDevices)

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
            changedDevices = session.processTriggers(deviceName, pinName,
                    pinValue)
            handleChangedDevices(senderQueue, changedDevices)
        elif pin["type"] == "output":
            intendedValue = intendedStates.get(deviceName, {}).\
                    get(pinName, pinValue)
            if pinValue != intendedValue:
                session.log(
                        device = deviceName,
                        pin = pinName,
                        severity = "warning",
                        message = "Wrong value of pin.")

                actions.setPin(senderQueue, deviceName, pinName, intendedValue)

    return ''



