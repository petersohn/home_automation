#!/usr/bin/python

import argparse
import json
import string
import sys

def main():
    parser = argparse.ArgumentParser(
            description="Generate files from config files.")
    parser.add_argument("configFile", nargs='*', type=argparse.FileType('r'),
            help='Config files.')
    parser.add_argument("--input", nargs='?', type=argparse.FileType('r'),
            required=True, help='The input file.')
    parser.add_argument("--output", nargs='?', type=argparse.FileType('w'),
            required=True, help='The output file.')
    parser.add_argument("--comment", nargs='?', help='The line comment string.')
    arguments = parser.parse_args()

    config = {}
    for file in arguments.configFile:
        contents = json.load(file)
        config.update(contents)

    input = arguments.input.read()
    output = arguments.output
    if arguments.comment is not None:
        output.write(arguments.comment + ' WARNING! This is an automatically ' +
                'generated file. Do not edit.\n\n')
    template = string.Template(input)
    result = template.substitute(config)
    output.write(result)
    output.flush()


if __name__ == "__main__":
    main()


