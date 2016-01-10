#!/usr/bin/env python2
# -*- coding: UTF-8 -*-

from cgi import escape
import sys

def run(environ, senderQueue, response):
    response.headers = [('Content-Type', 'text/html')]

    yield '<h1>FastCGI Environment</h1>'
    yield '<table>'
    for k, v in sorted(environ.items()):
         yield '<tr><th>%s</th><td>%s</td></tr>' % (escape(str(k)), escape(str(v)))
    yield '</table>'

