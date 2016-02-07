import sender

def setPin(senderQueue, deviceName, pinName, value):
    request = sender.Request(deviceName, "/" + pinName + "/" + (
            "1" if value else "0"))
    senderQueue.put(request)


def setPins(senderQueue, values):
    for deviceName, pinInfo in values.items():
        for pinName, value in pinInfo.items():
            setPin(senderQueue, deviceName, pinName, value)
