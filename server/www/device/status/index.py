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

    for pin in inputData["pins"]:
        if pin["type"] == "output" and \
                pin["value"] != session.getIntendedState(pin["name"]):
            session.log(
                    device = inputData["device"]["name"],
                    pin = pin["name"],
                    severity = "warning",
                    description = "Wrong value of pin.")

    return ''



