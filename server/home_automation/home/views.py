from home_automation import settings

from django.http.response import HttpResponse

import json
import os.path
import sys

executorClient = None


def init():
    scriptDirectory = os.path.dirname(os.path.abspath(__file__))
    sys.path.append(scriptDirectory + "/../../lib")

    import actions
    import executorClient

    global executorClient
    executorClient = ExecutorClient.ExecutorClient(
        settings.EXECUTOR_SOCKET_NAME)

    database.getSession().log("info", "Server instance started.")


def handleChangedDevices(executorClient, changedDevices):
    for changedDevice, changedPins in changedDevices.items():
        for changedPin, value in changedPins.items():
            actions.setPin(executorClient, changedDevice, changedPin, value)


def status(request):
    global exectutorClient
    if exectutorClient is None:
        init()

    import database
    import sender

    input = request.body.decode(
        request.encoding if request.encoding else "UTF-8")
    sys.stderr.write(input + "\n")
    inputData = json.loads(input)
    remoteAddress = request.environ["REMOTE_ADDR"]
    inputData["device"].setdefault("ip", remoteAddress)

    session = database.getSession()
    changedDevices = session.updateDevice(inputData)
    handleChangedDevices(exectutorClient, changedDevices)

    deviceName = inputData["device"]["name"]
    inputType = inputData.get("type", None)
    if inputType == "login":
        exectutorClient.send(sender.ClearDevice(deviceName))

    pins = inputData["pins"]

    intendedStates = session.getIntendedState(deviceName)
    for pin in pins:
        pinName = pin["name"]
        pinValue = pin["value"]
        if inputType == "event":
            changedDevices = session.processTriggers(
                deviceName, pinName, pinValue)
            handleChangedDevices(exectutorClient, changedDevices)
        elif pin["type"] == "output":
            intendedValue = intendedStates.get(deviceName, {}).\
                    get(pinName, pinValue)
            if pinValue != intendedValue:
                session.log(
                    device=deviceName,
                    pin=pinName,
                    severity="warning",
                    message="Wrong value of pin.")

                actions.setPin(
                    exectutorClient, deviceName, pinName, intendedValue)

    return HttpResponse("", content_type="text/plain")
