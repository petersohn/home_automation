import sender


def setPin(executorClient, deviceName, pinName, value):
    request = sender.Request(deviceName, "/" + pinName + "/" + (
            "1" if value else "0"))
    executorClient.send(request)


def setPins(executorClient, values):
    for deviceName, pinInfo in values.items():
        for pinName, value in pinInfo.items():
            setPin(executorClient, deviceName, pinName, value)
