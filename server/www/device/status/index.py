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

    for pin in inputData["pins"]:
        pinName = pin["name"]
        pinValue = pin["value"]
        if inputType == "event" and pin["type"] == "input":
            triggers = session.getTriggers(deviceName, pinName, pinValue)
            action = actions.Actions(senderQueue, session)
            for trigger in triggers:
                exec(trigger, {}, {"pinValue": pinValue, "action": action})
        else:
            intendedValue = session.getIntendedState(deviceName, pinName)
            if pin["type"] == "output" and pinValue != intendedValue:
                session.log(
                        device = deviceName,
                        pin = pinName,
                        severity = "warning",
                        description = "Wrong value of pin.")

                actions.setPin(senderQueue, deviceName, pinName, intendedValue)

    return ''



