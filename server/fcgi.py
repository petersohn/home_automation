#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

import flipflop
import imp
import multiprocessing
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


senderQueue = None
loadedModules = {}

def main(environ, start_response):
    global senderQueue
    global loadedModules
    try:
        fullpath = environ["DOCUMENT_ROOT"] + environ["SCRIPT_NAME"]
        name, ext = os.path.splitext(os.path.basename(fullpath))
        dirname = os.path.dirname(fullpath)
        file, pathname, description = imp.find_module(name, [dirname])
        try:
            module = loadedModules.get(fullpath, None)
            if module is None:
                module = imp.load_module(name, file, pathname, description)
                loadedModules[name] = module

            response = Response()
            result = module.run(environ, senderQueue, response)
            start_response(response.title, response.headers)
            return result
        finally:
            file.close()
    except Exception as e:
        session = database.getSession()
        session.log('error', 'Request failed: ' + e.__class__.__name__ + ": " +
                str(e))
        return handleGenericException(start_response)
    except:
        return handleGenericException(start_response)

if __name__ == "__main__":
    scriptDirectory = os.path.dirname(os.path.abspath(__file__))
    sys.path.append(scriptDirectory + "/lib")

    import database
    import sender

    senderQueue = multiprocessing.Queue()
    process = multiprocessing.Process(target = sender.runProcess,
            args = (senderQueue,))
    process.daemon = True
    process.start()

    database.getSession().log("info", "Server instance started.")
    server = flipflop.WSGIServer(main)
    server.run()

