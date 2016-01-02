#!/usr/bin/env python2
# -*- coding: UTF-8 -*-

import flup.server.fcgi
import imp
import os.path
import sys
import traceback

def main(environ, start_response):
    try:
        fullpath = environ["DOCUMENT_ROOT"] + environ["SCRIPT_NAME"]
        name, ext = os.path.splitext(os.path.basename(fullpath))
        dirname = os.path.dirname(fullpath)
        file, pathname, description = imp.find_module(name, [dirname])
        try:
            module = imp.load_module(name, file, pathname, description)
            return module.run(environ, start_response)
        finally:
            file.close()
    except:
        start_response('500 Internal Server Error', [('Content-Type', 'text/plain')])
        s = traceback.format_exc()
        sys.stderr.write(s + '\n')
        return s

if __name__ == "__main__":
    server = flup.server.fcgi.WSGIServer(main)
    server.run()

