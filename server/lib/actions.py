import sender

def setPin(senderQueue, deviceName, pinName, value):
    request = sender.Request(deviceName, "/" + pinName + "/" + (
            "1" if value else "0"))
    senderQueue.put(request)


class Actions:
    def __init__(self, senderQueue, session):
        self.senderQueue = senderQueue
        self.session = session


    def setControlGroup(self, name, value):
        self.session.setControlGroup(name, value)
        self._setPinsForControlGroup(name)


    def toggleControlGroup(self, name):
        self.session.toggleControlGroup(name)
        self._setPinsForControlGroup(name)


    def _setPinsForControlGroup(self, name):
        for (deviceName, pinName) in self.session.getPinsForControlGroup(name):
            setPin(self.senderQueue, deviceName, pinName,
                    self.session.getIntendedState(deviceName, pinName))
