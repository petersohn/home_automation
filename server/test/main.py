#!/usr/bin/env python2

import test_globals

import sys
import unittest

if __name__ == "__main__":
    sys.path.append(test_globals.libDirectory)
    test_globals.parseArguments()

    import tests

    unittest.main(module=tests)
