#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

import http.client
import json
import sys

import error

def sendData(host, port, data):
    try:
        connection = http.client.HTTPConnection(host, port=port, timeout=10)
        connection.request("POST", "/device/status/",
                body = json.dumps(data).encode("UTF-8"),
                headers = {"Content-Type": "application/json"})
        response = connection.getresponse()
    except:
        error.handleGenericException()
        return False

    if response.status >= 200 and response.status < 300:
        return True
    else:
        sys.stderr.write("<--- " + str(response.status) + " " + 
                response.reason + "\n")
        return False


