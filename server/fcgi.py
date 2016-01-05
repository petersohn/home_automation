#!/usr/bin/env python2
# -*- coding: UTF-8 -*-

import flup.server.fcgi
import imp
import os.path
import sys
import traceback

def handleGenericException(start_response):
    start_response('500 Internal Server Error', [('Content-Type', 'text/plain')])
    s = traceback.format_exc()
    sys.stderr.write(s + '\n')
    return s

class Response:
    title = "200 OK"
    headers = []

def main(environ, start_response):
    try:
        fullpath = environ["DOCUMENT_ROOT"] + environ["SCRIPT_NAME"]
        name, ext = os.path.splitext(os.path.basename(fullpath))
        dirname = os.path.dirname(fullpath)
        file, pathname, description = imp.find_module(name, [dirname])
        try:
            module = imp.load_module(name, file, pathname, description)
            response = Response()
            result = module.run(environ, response)
            start_response(response.title, response.headers)
            return result
        finally:
            file.close()
    except Exception as e:
        session = database.getSession()
        session.log('error', 'Request failed: ' + e.message)
        return handleGenericException(start_response)
    except:
        return handleGenericException(start_response)

if __name__ == "__main__":
    scriptDirectory = os.path.dirname(os.path.abspath(__file__))
    sys.path.append(scriptDirectory + "/lib")

    import database

    database.getSession().log("info", "Server instance started.")
    server = flup.server.fcgi.WSGIServer(main)
    server.run()

