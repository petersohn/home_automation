#!/usr/bin/env python2

import json
import sys

def main():
    output = []
    for file in sys.argv[1:]:
        f = open(file, 'r')
        contents = json.load(f)
        output.extend(contents)
    outfile = open('compile_commands.json', 'w')
    outfile.write(json.dumps(output, indent = 4, separators = (',', ': ')))
    outfile.write('\n')


if __name__ == "__main__":
    main()

