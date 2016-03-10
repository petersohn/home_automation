from home.models import Log, Pin


class Logger(object):
    @staticmethod
    def log(severity, message, device=None, pin=None):
        Log.objects.create(severity=severity.value,
                           message=message, device=device, pin=pin)


class DeviceService(object):
    def get_intended_pin_states(self, device):
        # FIXME: hacking
        class Var(object):
            def get(self, name):
                v = Variable.objects.get(name=name)
                return v.get()
            def set(self, name, value):
                v = Variable.objects.get(name=name)
                v.set(value)
            def toggle(self, name, modulo=2):
                v = Variable.objects.get(name=name)
                v.toggle(modulo)

        class Dev(object):
            def is_alive(self, name):
                d = Device.objects.get(name=name)
                return d.is_alive()

        result = {}
        output_pins = device.pin_set(kind=Pin.Kind.OUTPUT,
                                     expression_id__isnull=False)
        for output_pin in output_pins:
            for expression in output_pin.expression_set:
                result.setdefault(device.name, {})[pin.name] =\
                        eval(expression.value, {}, {
                            'var': Var(),
                            'dev': Dev()})
        return result


class TriggerService(object):
    def get_fireable_triggers(self, device, pin, pin_value):
        pass
