#!/usr/bin/env python3

import argparse
import json
import string
import sys
import os
import os.path


def decode(data):
    if data.__class__ == dict:
        raise Exception("Not supported!")
    if data.__class__ == list:
        result = "{"
        for element in data:
            result += decode(element) + ","
        result += "}"
        return result
    if data.__class__ == float or data.__class__ == int:
        return str(data)
    if data.__class__ == bool:
        return "true" if data else "false"
    # TODO: escape if necessary
    return '"' + data + '"'


FILE_SUFFIX = ".template"


def process_file(path, config, comment):
    if not path.endswith(FILE_SUFFIX):
        return

    with open(path) as f:
        input = f.read()

    with open(path[0:len(path) - len(FILE_SUFFIX)], "w") as output:
        if comment is not None:
            output.write(
                comment + ' WARNING! This is an automatically ' +
                'generated file. Do not edit.\n\n')
        template = string.Template(input)
        result = template.substitute(config)
        output.write(result)


def main():
    print(sys.argv)
    parser = argparse.ArgumentParser(
            description="Generate files from config files.")
    parser.add_argument("--configFile", nargs='*', type=argparse.FileType('r'),
                        help='Config files.')
    parser.add_argument("--input", nargs='+', type=str, required=True,
                        help='The path for the input files.')
    parser.add_argument("--comment", nargs='?',
                        help='The line comment string.')
    arguments = parser.parse_args()

    config = {}
    for file in arguments.configFile:
        contents = json.load(file)
        config.update(contents)

    for key, value in config.items():
        config[key] = decode(value)

    for path in arguments.input:
        if os.path.isdir(path):
            for dirpath, dirnames, filenames in os.walk(path):
                for filename in filenames:
                    process_file(
                        os.path.join(dirpath, filename), config,
                        arguments.comment)
        else:
            process_file(path, config, arguments.comment)


if __name__ == "__main__":
    main()
