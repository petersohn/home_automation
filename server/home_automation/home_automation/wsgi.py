#!/usr/bin/env python3

import os
import sys

scriptDirectory = os.path.dirname(os.path.abspath(__file__))
sys.path.append(scriptDirectory + "/..")
sys.path.append(scriptDirectory)

from django.core.wsgi import get_wsgi_application
from flipflop2 import WSGIServer

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "home_automation.settings")

my_application = get_wsgi_application()


def application(environ, start_response):
    # maxlen = max([len(key) for key, value in environ.items()])
    # for k, v in sorted(environ.items()):
        # sys.stderr.write(
            # "{{0:{}}}".format(maxlen + 1).format(str(k)) + " --> " +
            # str(v) + "\n")
    environ["SCRIPT_NAME"] = ""
    return my_application(environ, start_response)

if __name__ == '__main__':
    server = WSGIServer(application)
    server.run()
