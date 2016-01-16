#!/usr/bin/env python3

import test_globals

import sys
import unittest

if __name__ == "__main__":
    sys.path.append(test_globals.libDirectory)
    args = test_globals.parseArguments()

    import tests

    argv = sys.argv[0:1]
    argv.extend(args)
    unittest.main(module=tests, argv=argv)
