#!/usr/bin/env python2
# -*- coding: UTF-8 -*-

import database

import json
import sys

def run(environ, response):
    response.headers = [('Content-Type', 'text/plain')]
    input = environ["wsgi.input"]
    inputData = json.load(input)
    database.getSession().updateDevice(inputData)
    return ''



