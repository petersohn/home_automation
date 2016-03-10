from home import models


class Logger(object):
    @staticmethod
    def log(severity, message, device=None, pin=None):
        models.Log.objects.create(
            severity=severity.value, message=message, device=device, pin=pin)


class DeviceService(object):
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


class TriggerService(object):
    def process_triggers(self, pin, pin_value):
        pass
