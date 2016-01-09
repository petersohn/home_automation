#!/usr/bin/env python2
# -*- coding: UTF-8 -*-

import database

import json
import sys

def run(environ, response):
    response.headers = [('Content-Type', 'text/plain')]
    input = environ["wsgi.input"]
    inputData = json.load(input)
    session = database.getSession()

    session.updateDevice(inputData)

    deviceName = inputData["device"]["name"]
    for pin in inputData["pins"]:
        pinName = pin["name"]
        if pin["type"] == "output" and \
                pin["value"] != session.getIntendedState(deviceName, pinName):
            session.log(
                    device = deviceName,
                    pin = pinName,
                    severity = "warning",
                    description = "Wrong value of pin.")

    return ''



