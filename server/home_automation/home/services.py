from home import models


class Logger(object):
    @staticmethod
    def log(severity, message, device=None, pin=None):
        models.Log.objects.create(
            severity=severity.value, message=message, device=device, pin=pin)


class ExpressionEvaluator(object):
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

        def count_alive(self):
            return models.Device.objects.count_alive()

        def count_dead(self):
            return models.Device.objects.count_dead()

    VARIABLE_PROXY = VariableProxy()
    DEVICE_PROXY = DeviceProxy()

    def evaluate(self, expression):
        return eval(expression, {}, {
            'var': self.VARIABLE_PROXY,
            'dev': self.DEVICE_PROXY})


def _get_changed_states(initial_states, new_states):
    result = {}
    for device_name, pin_info in new_states.items():
        for pin_name, value in pin_info.items():
            if value != initial_states.get(device_name, {}).get(pin_name):
                result.setdefault(device_name, {})[pin_name] = value

    return result


class DeviceService(ExpressionEvaluator):
    def get_intended_pin_states(self, device=None):
        extra_args = {}
        if device is not None:
            extra_args['device'] = device
        else:
            extra_args['device__last_seen__gte'] = (
                models.Device.heartbeat_time_limit())
        output_pins = models.Pin.objects.select_related(
            'expression').select_related('device').filter(
                expression__isnull=False, kind=models.Pin.Kind.OUTPUT.value,
                **extra_args)
        result = {}
        for output_pin in output_pins:
            result.setdefault(output_pin.device.name, {})[output_pin.name] = (
                self.evaluate(output_pin.expression.value))
        return result

    def update_device_and_pins(self, data):
        input_type = data.get('type', None)
        is_login = input_type is not None and input_type == 'login'
        initial_state = self.get_intended_pin_states() if not is_login else {}
        device_data = data['device']
        device, is_created = models.Device.objects.get_or_create(
            name=device_data['name'], defaults={
                'name': device_data['name'], 'ip_address': device_data['ip'],
                'port': device_data['port'],
                'version': device_data.get('version', 1)})

        if is_created:
            Logger.log(models.Log.Severity.INFO, 'Found new device',
                       device=device)
        else:
            device.update_from_json(device_data)
            if not device.is_alive():
                Logger.log(models.Log.Severity.INFO, 'Lost device reappeared',
                           device=device)
            elif is_login:
                Logger.log(models.Log.Severity.WARNING, 'Device is restarted',
                           device=device)

        if input_type != 'event':
            self.update_pins(device, data['pins'])

        new_state = self.get_intended_pin_states()
        return _get_changed_states(initial_state, new_state)

    def update_pins(self, device, pins):
        names = []
        for pin in pins:
            names.append(pin['name'])
            models.Pin.objects.update_or_create(
                device=device, name=pin['name'], defaults={
                    'name': pin['name'],
                    'kind': models.Pin.Kind.from_string(pin['type']).value})
        models.Pin.objects.filter(device=device).exclude(
            name__in=names).delete()


class TriggerService(ExpressionEvaluator):
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
        input_triggers = (
            models.InputTrigger.objects.select_related('expression').
            select_related('pin').select_related('pin__device').filter(
                edge__in=[models.InputTrigger.Edge.BOTH.value, edge.value],
                pin=pin))

        initial_states = self.device_service.get_intended_pin_states()
        for input_trigger in input_triggers:
            exec(input_trigger.expression.value, {}, {
                "pin": Pin(input_trigger.pin.device.name,
                           input_trigger.pin.name, pin_value),
                "var": self.VARIABLE_PROXY,
                "dev": self.DEVICE_PROXY,
                "log": Logger()})
        new_states = self.device_service.get_intended_pin_states()
        return _get_changed_states(initial_states, new_states)
