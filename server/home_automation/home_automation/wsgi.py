#!/usr/bin/env python3

import os
import sys

from django.core.wsgi import get_wsgi_application
from flipflop2 import WSGIServer

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "home_automation.settings")

scriptDirectory = os.path.dirname(os.path.abspath(__file__))
sys.path.append(scriptDirectory + "/..")

application = get_wsgi_application()


if __name__ == '__main__':
    server = WSGIServer(application)
    server.run()
