from home import models


class Logger(object):
    @staticmethod
    def log(severity, message, device=None, pin=None):
        models.Log.objects.create(
            severity=severity.value, message=message, device=device, pin=pin)


class ExpressionService(object):
    # FIXME: hacking
    class VariableProxy(object):
        def get(self, name):
            v = models.Variable.objects.get(name=name)
            return v.get()

        def set(self, name, value):
            v = models.Variable.objects.get(name=name)
            v.semodels.t(value)

        def toggle(self, name, modulo=2):
            v = models.Variable.objects.get(name=name)
            v.toggle(modulo)

    class DeviceProxy(object):
        def is_alive(self, name):
            d = models.Device.objects.get(name=name)
            return d.is_alive()

    VARIABLE_PROXY = VariableProxy()
    DEVICE_PROXY = DeviceProxy()


def _get_changed_states(initial_states, new_states):
    result = {}
    for device_name, pin_info in new_states.items():
        for pin_name, value in pin_info.items():
            if value != initial_states.get(device_name, {}).get(pin_name):
                result.setdefault(device_name, {})[pin_name] = value

    return result


class DeviceService(ExpressionService):
    def get_intended_pin_states(self, device=None):
        extra_args = {}
        if device is not None:
            extra_args['device'] = device
        output_pins = models.Pin.objects.select_related(
            'expression').select_related('device').filter(
                expression__isnull=False, kind=models.Pin.Kind.OUTPUT.value,
                **extra_args)
        result = {}
        for output_pin in output_pins:
            result.setdefault(output_pin.device.name, {})[output_pin.name] = (
                eval(output_pin.expression.value, {}, {
                    'var': self.VARIABLE_PROXY,
                    'dev': self.DEVICE_PROXY}))
        return result


class TriggerService(ExpressionService):
    def __init__(self, device_service=DeviceService()):
        self.device_service = device_service

    def process_triggers(self, pin, pin_value):
        class Pin:
            def __init__(self, device, pin, value):
                self.device = device
                self.pin = pin
                self.value = value

        edge = models.InputTrigger.Edge.RISING if pin_value else \
            models.InputTrigger.Edge.FALLING
        input_triggers = models.InputTrigger.filter(
            edge__in=[models.InputTrigger.Edge.BOTH, edge],
            pin=pin).select_related('expression').select_related('pin')

        initial_states = self.device_service.get_intended_pin_states()
        for input_trigger in input_triggers:
            exec(input_trigger.expression.value, {}, {
                "pin": Pin(input_trigger.device.name, input_trigger.pin.name,
                           pin_value),
                "var": self.VARIABLE_PROXY,
                "dev": self.DEVICE_PROXY,
                "log": self.Logger})
        new_states = self.device_service.get_intended_pin_states()
        return _get_changed_states(initial_states, new_states)
