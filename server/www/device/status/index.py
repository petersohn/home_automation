#!/usr/bin/env python2
# -*- coding: UTF-8 -*-

import sys

def run(environ, response):
    response.headers = [('Content-Type', 'text/plain')]
    input = environ["wsgi.input"]

    s = input.read()
    sys.stderr.write(s + '\n')
    return s



