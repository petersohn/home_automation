from home_automation import settings

from django.http.response import HttpResponse
from django.views.generic import View

import json
import os.path
import sys

executor_client = None


def init():
    scriptDirectory = os.path.dirname(os.path.abspath(__file__))
    sys.path.append(scriptDirectory + "/../../lib")

    import actions
    import executorClient

    global executor_client
    executor_client = ExecutorClient.ExecutorClient(
        settings.EXECUTOR_SOCKET_NAME)

    database.getSession().log("info", "Server instance started.")


def handle_changed_devices(executor_client, changed_devices):
    for changedDevice, changedPins in changed_devices.items():
        for changedPin, value in changedPins.items():
            actions.setPin(executor_client, changedDevice, changedPin, value)


class StatusView(View):
    def __init__(self):
        self.device_service = services.DeviceService()

    http_method_names = ['POST'] # FIXME: is it really needed?

    def post(request, *args, **kwargs):
        data = json.loads(request.body.decode(
            request.encoding if request.encoding else 'UTF-8'))
        remote_ip_address = request.environ['REMOTE_ADDR']
        data['device'].setDefault('ip', remote_ip_address)
        changed_devices = device_service.update_device_and_pins(data)
        handle_changed_devices(executor_client, changed_devices)
        device_name = data['device']['name']
        input_type = data.get('type', None)
        if input_type == 'login':
            executor_client.send(sender.ClearDevice(device_name))

        if inputType == "event":
            pins = data['pins']
            for pin in pins:
                pinName = pin["name"]
                pinValue = pin["value"]
                changedDevices = session.processTriggers(
                    deviceName, pinName, pinValue)
                handleChangedDevices(exectutorClient, changedDevices)
        return HttpResponse("", content_type="text/plain")


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
