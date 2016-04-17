#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

import sys
import traceback


def handleGenericException():
    s = traceback.format_exc()
    sys.stderr.write(s + '\n')
