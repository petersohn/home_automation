#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

class Pin:
    def __init__(self, name, output, number):
        self.name = name
        self.output = output
        self.number = number


def initPins(pinConfig):
    return [Pin(pin["name"], pin["output"], pin["number"]) for pin in pinConfig]


