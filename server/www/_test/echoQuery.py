#!/usr/bin/env python2
# -*- coding: UTF-8 -*-

import urllib

def run(environ, senderQueue, response):
    response.headers = [('Content-Type', 'text/plain')]
    return urllib.unquote(environ["QUERY_STRING"])

