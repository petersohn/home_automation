#!/usr/bin/env python3

import test_globals

import sys
import unittest

if __name__ == "__main__":
    sys.path.append(test_globals.libDirectory)
    import tests
    unittest.main(module=tests, argv=sys.argv)
