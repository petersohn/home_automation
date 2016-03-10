from django.http.response import HttpResponse
import multiprocessing
import json
import os.path
import sys

senderQueue = None
process = None


def init():
    scriptDirectory = os.path.dirname(os.path.abspath(__file__))
    sys.path.append(scriptDirectory + "/../../lib")

    global senderQueue
    global process

    senderQueue = multiprocessing.Queue()
    process = multiprocessing.Process(target=sender.runProcess,
        args=(senderQueue,))
    process.daemon = True
    process.start()

    database.getSession().log("info", "Server instance started.")


def handleChangedDevices(senderQueue, changedDevices):
    for changedDevice, changedPins in changedDevices.items():
        for changedPin, value in changedPins.items():
            actions.setPin(senderQueue, changedDevice, changedPin, value)


def status(request):
    global senderQueue
    if senderQueue is None:
        init()

    import database
    import sender

    input = request.body.decode(request.encoding if request.encoding else "UTF-8")
    sys.stderr.write(input + "\n")
    inputData = json.loads(input)
    remoteAddress = request.environ["REMOTE_ADDR"]
    inputData["device"].setdefault("ip", remoteAddress)

    session = database.getSession()
    changedDevices = session.updateDevice(inputData)
    handleChangedDevices(senderQueue, changedDevices)

    deviceName = inputData["device"]["name"]
    inputType = inputData.get("type", None)
    if inputType == "login":
        senderQueue.put(sender.ClearDevice(deviceName))

    pins = inputData["pins"]

    intendedStates = session.getIntendedState(deviceName)
    for pin in pins:
        pinName = pin["name"]
        pinValue = pin["value"]
        if inputType == "event":
            changedDevices = session.processTriggers(deviceName, pinName,
                    pinValue)
            handleChangedDevices(senderQueue, changedDevices)
        elif pin["type"] == "output":
            intendedValue = intendedStates.get(deviceName, {}).\
                    get(pinName, pinValue)
            if pinValue != intendedValue:
                session.log(
                        device = deviceName,
                        pin = pinName,
                        severity = "warning",
                        message = "Wrong value of pin.")

                actions.setPin(senderQueue, deviceName, pinName, intendedValue)

    return HttpResponse("", content_type = "text/plain")
