from home_automation import settings
from home import models, services

from django.http.response import HttpResponse
from django.views.generic import View
# from django.views.decorators.csrf import csrf_exempt

import json
import os.path
import sys
import traceback

scriptDirectory = os.path.dirname(os.path.abspath(__file__))
sys.path.append(scriptDirectory + "/../../lib")

import actions
import ExecutorClient
import sender

executor_client = None


def init():
    global executor_client
    executor_client = ExecutorClient.ExecutorClient(
        settings.EXECUTOR_SOCKET_NAME)

    services.Logger().log(models.Log.Severity.INFO, "Server instance started.")


def handle_changed_devices(executor_client, changed_devices):
    for changedDevice, changedPins in changed_devices.items():
        for changedPin, value in changedPins.items():
            actions.setPin(executor_client, changedDevice, changedPin, value)


class StatusView(View):
    def __init__(self):
        global executor_client
        if executor_client is None:
            init()
        self.device_service = services.DeviceService()

    # @csrf_exempt
    def post(self, request, *args, **kwargs):
        global executor_client
        try:
            data = json.loads(request.body.decode(
                request.encoding if request.encoding else 'UTF-8'))
            remote_ip_address = request.environ['REMOTE_ADDR']
            data['device'].setdefault('ip', remote_ip_address)
            changed_devices = self.device_service.update_device_and_pins(data)
            handle_changed_devices(executor_client, changed_devices)
            device_name = data['device']['name']
            input_type = data.get('type', None)
            if input_type == 'login':
                executor_client.send(sender.ClearDevice(device_name))

            if input_type == "event":
                pins = data['pins']
                for pin in pins:
                    pin_name = pin["name"]
                    pin_value = pin["value"]
                    pin_data = models.Pin.objects.get(
                        name=pin_name, device__name=device_name)
                    changedDevices = self.device_service.process_triggers(
                        pin_data, pin_value)
                    handle_changed_devices(executor_client, changedDevices)

            return HttpResponse("", content_type="text/plain")
        except:
            sys.stderr.write(traceback.format_exc() + "\n")
            raise
