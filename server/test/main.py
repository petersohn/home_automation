#!/usr/bin/env python2

import globals

import sys
import unittest

if __name__ == "__main__":
    sys.path.append(globals.libDirectory)
    globals.parseArguments()

    import tests

    unittest.main(module=tests)
