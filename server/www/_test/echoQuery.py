#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

import urllib

def run(environ, senderQueue, response):
    response.headers = [('Content-Type', 'text/plain')]
    return urllib.parse.unquote(environ["QUERY_STRING"])

