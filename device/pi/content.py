#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

import error
import gpio


class Content:
    def __init__(self, deviceName, port, pins):
        self.deviceName = deviceName
        self.port = port
        self.pins = pins

    def _getDeviceInfo(self):
        return {"name": self.deviceName, "port": self.port, "version": 1}

    def _getPinInfo(self, pin):
        return {"type": "output" if pin.output else "input",
                "name": pin.name,
                "value": gpio.getPinValue(pin)}

    def _getModifiedPinInfo(self):
        result = []
        for pin in self.pins:
            if pin.output:
                continue
            value = gpio.getPinValue(pin)
            if pin.value != value:
                pin.value = value
                result.append(self._getPinInfo(pin))

        return result

    def getFullStatus(self, type=None):
        pinData = []
        for pin in self.pins:
            pinData.append(self._getPinInfo(pin))
        result = {"device": self._getDeviceInfo(), "pins": pinData}
        if type is not None:
            result["type"] = type

        return result

    def getModifiedPinsContent(self, type):
        modifiedPins = self._getModifiedPinInfo()
        if len(modifiedPins) != 0:
            return {"device": self._getDeviceInfo(), "pins": modifiedPins,
                    "type": type}
        else:
            return None

    def getContent(self, path):
        tokens = path.split("/")
        numTokens = len(tokens)

        if numTokens == 0 or len(tokens[0]) != 0:
            return None

        if numTokens == 2:
            return self.getFullStatus()

        pinName = tokens[1]
        try:
            [pin] = [pin for pin in self.pins if pin.name == pinName]
        except:
            error.handleGenericException()
            return None

        if numTokens > 2:
            if not pin.output:
                return None

            try:
                value = int(tokens[2])
                gpio.setPinValue(pin, value)
            except:
                error.handleGenericException()
                return None

        return self._getPinInfo(pin)
