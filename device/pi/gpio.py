#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

import RPi.GPIO as GPIO


def setupPins(pins, changeHandler):
    for pin in pins:
        GPIO.setup(pin.number, GPIO.OUT if pin.output else GPIO.IN)
        pin.value = GPIO.input(pin.number)
        if not pin.output:
            GPIO.add_event_detect(
                pin.number, GPIO.BOTH, callback=(lambda pin: changeHandler()),
                bouncetime=100)


def getPinValue(pin):
    return GPIO.input(pin.number)


def setPinValue(pin, value):
    GPIO.output(pin.number, value)


class Context:
    def __enter__(self):
        GPIO.setmode(GPIO.BOARD)

    def __exit__(self, type, value, traceback):
        GPIO.cleanup()
        return None
