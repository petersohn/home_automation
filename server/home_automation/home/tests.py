from home import config, models, services

import django.utils.timezone
from django.test import TestCase

import unittest.mock
from unittest.mock import call


class VariableTest(TestCase):
    FOO_NAME = 'Foo'
    BAR_NAME = 'Bar'
    FOO_VALUE = 1
    BAR_VALUE = 3

    def setUp(self):
        models.Variable.objects.create(
            name=self.FOO_NAME, value=self.FOO_VALUE)
        models.Variable.objects.create(
            name=self.BAR_NAME, value=self.BAR_VALUE)

    def test_get_value_of_variable(self):
        foo = models.Variable.objects.get(name=self.FOO_NAME)
        bar = models.Variable.objects.get(name=self.BAR_NAME)
        self.assertEqual(self.FOO_VALUE, foo.get())
        self.assertEqual(self.BAR_VALUE, bar.get())

    def test_set_value_of_variable(self):
        FOO_NEW_VALUE = 5
        foo = models.Variable.objects.get(name=self.FOO_NAME)
        foo.set(FOO_NEW_VALUE)
        foo = None
        foo = models.Variable.objects.get(name=self.FOO_NAME)
        self.assertEqual(FOO_NEW_VALUE, foo.get())

    def test_toggle_value_of_variable(self):
        foo = models.Variable.objects.get(name=self.FOO_NAME)
        foo.toggle()
        self.assertEqual(0, foo.get())

    def test_toggle_value_of_variable_with_modulo(self):
        foo = models.Variable.objects.get(name=self.FOO_NAME)
        foo.toggle(3)
        self.assertEqual(2, foo.get())


class DeviceTest(TestCase):
    DEVICE1_NAME = 'Device1'
    DEVICE2_NAME = 'Device2'
    DEVICE3_NAME = 'Device3'
    DEVICE_VERSION = 1

    def setUp(self):
        models.Device.objects.create(
            name=self.DEVICE1_NAME, ip_address='127.0.0.1',
            port=9999, version=self.DEVICE_VERSION)
        models.Device.objects.create(
            name=self.DEVICE2_NAME, ip_address='127.0.0.1',
            port=9998, version=self.DEVICE_VERSION)
        models.Device.objects.create(
            name=self.DEVICE3_NAME, ip_address='127.0.0.1',
            port=9997, version=self.DEVICE_VERSION)
        models.Device.objects.filter(name=self.DEVICE2_NAME).update(
            last_seen=django.utils.timezone.now() - (
                2 * config.device_heartbeat_timeout))

    def test_is_alive(self):
        device1 = models.Device.objects.get(name=self.DEVICE1_NAME)
        self.assertTrue(device1.is_alive())
        device2 = models.Device.objects.get(name=self.DEVICE2_NAME)
        self.assertFalse(device2.is_alive())

    def test_count_alive(self):
        self.assertEqual(2, models.Device.objects.count_alive())

    def test_count_dead(self):
       self.assertEqual(1, models.Device.objects.count_dead())


class LoggerTest(TestCase):
    def setUp(self):
        self.device = models.Device.objects.create(
            name='Device', ip_address='127.0.0.1',
            port=9999, version=1)
        self.pin = models.Pin.objects.create(
            name='Pin', device=self.device, kind=models.Pin.Kind.INPUT.value,
            expression=None)

    def test_logger_log(self):
        MESSAGE1 = "Foo"
        MESSAGE2 = "Bar"
        services.Logger.log(severity=models.Log.Severity.WARNING,
                            message=MESSAGE1)
        services.Logger.log(severity=models.Log.Severity.INFO,
                            message=MESSAGE2)
        self.assertEqual(2, models.Log.objects.count())
        expected_entries = set([(models.Log.Severity.WARNING.value, MESSAGE1),
                                (models.Log.Severity.INFO.value, MESSAGE2)])
        self.assertSetEqual(expected_entries, set(map(
            lambda e: (e.severity, e.message), models.Log.objects.all())))

    def test_logger_with_device_and_pin(self):
        MESSAGE1 = 'FooBar'
        MESSAGE2 = 'FooBar2'
        services.Logger.log(severity=models.Log.Severity.INFO,
                            message=MESSAGE1, device=self.device)
        services.Logger.log(severity=models.Log.Severity.INFO,
                            message=MESSAGE2, device=self.device, pin=self.pin)

        expected_entries = set([(MESSAGE1, self.device, None),
                                (MESSAGE2, self.device, self.pin)])
        self.assertSetEqual(expected_entries, set(map(
            lambda e: (e.message, e.device, e.pin), models.Log.objects.all())))


class DeviceServiceTest(TestCase):
    DEVICE_NAME = 'Device'
    SECOND_DEVICE_NAME = 'SecondDevice'
    FIRST_TRUE_PIN_NAME = 'TruePin'
    FIRST_FALSE_PIN_NAME = 'FalsePin'
    SECOND_TRUE_PIN_NAME = 'SecondTruePin'
    SECOND_FALSE_PIN_NAME = 'SecondFalsePin'

    def setUp(self):
        self.device = models.Device.objects.create(
            name=self.DEVICE_NAME, ip_address='1.1.1.1', port=9999, version=1)
        self.true_expression = models.Expression.objects.create(value='True')
        self.false_expression = models.Expression.objects.create(value='False')
        self.variable_false_expression = models.Expression.objects.create(
            value='dev.getFalse()')
        self.variable_true_expression = models.Expression.objects.create(
            value='var.getTrue()')

    def test_get_intended_pin_state_constant_true(self):
        TRUE_PIN_NAME = 'TruePin'
        self.pin = models.Pin.objects.create(
            name=TRUE_PIN_NAME, device=self.device,
            kind=models.Pin.Kind.OUTPUT.value, expression=self.true_expression)
        device_service = services.DeviceService()
        result = device_service.get_intended_pin_states(None)
        self.assertEqual({self.DEVICE_NAME: {TRUE_PIN_NAME: True}}, result)

    def test_get_intended_pin_state_constant_false(self):
        FALSE_PIN_NAME = 'FalsePin'
        self.pin = models.Pin.objects.create(
            name=FALSE_PIN_NAME, device=self.device,
            kind=models.Pin.Kind.OUTPUT.value,
            expression=self.false_expression)
        device_service = services.DeviceService()
        result = device_service.get_intended_pin_states()
        self.assertEqual({self.DEVICE_NAME: {FALSE_PIN_NAME: False}}, result)

    def test_get_intended_pin_state_depending_on_variable(self):
        TRUE_PIN_NAME = 'TruePin'
        mock_variable_proxy = unittest.mock.Mock()
        mock_variable_proxy.getFalse.return_value = False
        mock_variable_proxy.getTrue.return_value = True
        services.DeviceService.VARIABLE_PROXY = mock_variable_proxy
        self.pin = models.Pin.objects.create(
            name=TRUE_PIN_NAME, device=self.device,
            kind=models.Pin.Kind.OUTPUT.value,
            expression=self.variable_true_expression)
        device_service = services.DeviceService()
        result = device_service.get_intended_pin_states()
        self.assertEqual({self.DEVICE_NAME: {TRUE_PIN_NAME: True}}, result)

    def test_get_intended_pin_state_depending_on_device(self):
        FALSE_PIN_NAME = 'FalsePin'
        mock_device_proxy = unittest.mock.Mock()
        mock_device_proxy.getFalse.return_value = False
        mock_device_proxy.getTrue.return_value = True
        services.DeviceService.DEVICE_PROXY = mock_device_proxy
        self.pin = models.Pin.objects.create(
            name=FALSE_PIN_NAME, device=self.device,
            kind=models.Pin.Kind.OUTPUT.value,
            expression=self.variable_false_expression)
        device_service = services.DeviceService()
        result = device_service.get_intended_pin_states()
        self.assertEqual({self.DEVICE_NAME: {FALSE_PIN_NAME: False}}, result)

    def test_get_intended_pin_state_does_not_return_input_pins(self):
        TRUE_PIN_NAME = 'TruePin'
        self.input_pin = models.Pin.objects.create(
            name='InputPin', device=self.device,
            kind=models.Pin.Kind.INPUT.value, expression=None)
        self.pin = models.Pin.objects.create(
            name=TRUE_PIN_NAME, device=self.device,
            kind=models.Pin.Kind.OUTPUT.value, expression=self.true_expression)
        device_service = services.DeviceService()
        result = device_service.get_intended_pin_states(None)
        self.assertEqual({self.DEVICE_NAME: {TRUE_PIN_NAME: True}}, result)

    def test_get_intended_pin_state_does_not_return_inactive_pins(self):
        TRUE_PIN_NAME = 'TruePin'
        self.inacive_pin = models.Pin.objects.create(
            name='InactivePin', device=self.device,
            kind=models.Pin.Kind.OUTPUT.value, expression=None)
        self.pin = models.Pin.objects.create(
            name=TRUE_PIN_NAME, device=self.device,
            kind=models.Pin.Kind.OUTPUT.value, expression=self.true_expression)
        device_service = services.DeviceService()
        result = device_service.get_intended_pin_states(None)
        self.assertEqual({self.DEVICE_NAME: {TRUE_PIN_NAME: True}}, result)

    def set_up_multuple_devices(self):
        self.second_device = models.Device.objects.create(
            name=self.SECOND_DEVICE_NAME, ip_address='1.1.1.1', port=9998, version=1)
        first_true_pin = models.Pin.objects.create(
            name=self.FIRST_TRUE_PIN_NAME, device=self.device,
            kind=models.Pin.Kind.OUTPUT.value,
            expression=self.true_expression)
        first_false_pin = models.Pin.objects.create(
            name=self.FIRST_FALSE_PIN_NAME, device=self.device,
            kind=models.Pin.Kind.OUTPUT.value,
            expression=self.false_expression)
        second_true_pin = models.Pin.objects.create(
            name=self.SECOND_TRUE_PIN_NAME, device=self.second_device,
            kind=models.Pin.Kind.OUTPUT.value,
            expression=self.true_expression)
        second_false_pin = models.Pin.objects.create(
            name=self.SECOND_FALSE_PIN_NAME, device=self.second_device,
            kind=models.Pin.Kind.OUTPUT.value,
            expression=self.false_expression)

    def test_get_intended_pin_state_multiple_devices(self):
        self.set_up_multuple_devices()
        device_service = services.DeviceService()
        result = device_service.get_intended_pin_states()
        self.assertEqual({self.DEVICE_NAME: {self.FIRST_TRUE_PIN_NAME: True,
                                             self.FIRST_FALSE_PIN_NAME: False},
                          self.SECOND_DEVICE_NAME: {
                              self.SECOND_TRUE_PIN_NAME: True,
                              self.SECOND_FALSE_PIN_NAME: False}},
                         result)

    def test_get_intended_pin_state_returns_only_for_given_device(self):
        self.set_up_multuple_devices()
        device_service = services.DeviceService()
        result = device_service.get_intended_pin_states(self.second_device)
        self.assertEqual({self.SECOND_DEVICE_NAME: {
                              self.SECOND_TRUE_PIN_NAME: True,
                              self.SECOND_FALSE_PIN_NAME: False}},
                         result)

    def test_get_intended_pin_state_returns_only_active_devices(self):
        self.set_up_multuple_devices()
        models.Device.objects.filter(name=self.DEVICE_NAME).update(
            last_seen=django.utils.timezone.now() - (
                2 * config.device_heartbeat_timeout))
        device_service = services.DeviceService()
        result = device_service.get_intended_pin_states()
        self.assertEqual({self.SECOND_DEVICE_NAME: {
                              self.SECOND_TRUE_PIN_NAME: True,
                              self.SECOND_FALSE_PIN_NAME: False}},
                         result)

    def create_input_data(
            self, name, ip, port, input_type="heartbeat", pins=[]):
        device = {"name": name, "ip": ip, "port": port}
        pins = [{"name": pin_name, "type": type} for (pin_name, type) in pins]
        return {"device": device, "pins": pins, "type": input_type}

    def test_update_device_and_pins_add_new_device(self):
        NEW_DEVICE_NAME = "NewDevice"
        NEW_DEVICE_IP = "192.168.1.10"
        NEW_DEVICE_PORT = 80
        data = self.create_input_data(
            NEW_DEVICE_NAME, NEW_DEVICE_IP, NEW_DEVICE_PORT)

        initial_device_count = models.Device.objects.count()
        device_service = services.DeviceService()
        device_service.update_device_and_pins(data)
        final_device_count = models.Device.objects.count()

        self.assertEqual(initial_device_count + 1, final_device_count)
        added_device = models.Device.objects.get(name=NEW_DEVICE_NAME)
        self.assertEqual(NEW_DEVICE_NAME, added_device.name)
        self.assertEqual(NEW_DEVICE_IP, added_device.ip_address)
        self.assertEqual(NEW_DEVICE_PORT, added_device.port)

    def test_update_device_and_pins_update_existing_device(self):
        NEW_DEVICE_NAME = self.DEVICE_NAME
        NEW_DEVICE_IP = "192.168.1.10"
        NEW_DEVICE_PORT = 80
        data = self.create_input_data(
            NEW_DEVICE_NAME, NEW_DEVICE_IP, NEW_DEVICE_PORT)

        initial_device_count = models.Device.objects.count()
        device_service = services.DeviceService()
        device_service.update_device_and_pins(data)
        final_device_count = models.Device.objects.count()

        self.assertEqual(initial_device_count, final_device_count)
        added_device = models.Device.objects.get(name=NEW_DEVICE_NAME)
        self.assertEqual(NEW_DEVICE_NAME, added_device.name)
        self.assertEqual(NEW_DEVICE_IP, added_device.ip_address)
        self.assertEqual(NEW_DEVICE_PORT, added_device.port)

    def test_update_device_and_pins_set_pins(self):
        NEW_DEVICE_NAME = "NewDevice"
        NEW_DEVICE_IP = "192.168.1.10"
        NEW_DEVICE_PORT = 80
        NEW_INPUT_PIN_NAME = "NewInputPin"
        NEW_OUTPUT_PIN_NAME = "NewOutputPin"

        data = self.create_input_data(
            NEW_DEVICE_NAME, NEW_DEVICE_IP, NEW_DEVICE_PORT,
            pins=[(NEW_INPUT_PIN_NAME, "input"),
                  (NEW_OUTPUT_PIN_NAME, "output")])

        initial_device_count = models.Device.objects.count()
        initial_pin_count = models.Pin.objects.count()
        device_service = services.DeviceService()
        device_service.update_device_and_pins(data)
        final_device_count = models.Device.objects.count()
        final_pin_count = models.Pin.objects.count()

        self.assertEqual(initial_device_count + 1, final_device_count)
        self.assertEqual(initial_pin_count + 2, final_pin_count)
        added_device = models.Device.objects.get(name=NEW_DEVICE_NAME)
        added_pins = models.Pin.objects.filter(device=added_device)
        expected_entries = set([
            (NEW_INPUT_PIN_NAME, models.Pin.Kind.INPUT.value),
            (NEW_OUTPUT_PIN_NAME, models.Pin.Kind.OUTPUT.value)])
        self.assertSetEqual(expected_entries, set(map(
            lambda e: (e.name, e.kind), added_pins)))

    def test_update_device_and_pins_set_pins_for_multiple_devices(self):
        NEW_DEVICE1_NAME = "NewDevice1"
        NEW_DEVICE1_IP = "192.168.1.12"
        NEW_DEVICE1_PORT = 80
        NEW_DEVICE2_NAME = "NewDevice2"
        NEW_DEVICE2_IP = "192.168.1.10"
        NEW_DEVICE2_PORT = 83
        NEW_INPUT_PIN1_NAME = "NewInputPin1"
        NEW_INPUT_PIN2_NAME = "NewInputPin2"
        NEW_OUTPUT_PIN_NAME = "NewOutputPin"

        device_service = services.DeviceService()
        initial_device_count = models.Device.objects.count()
        initial_pin_count = models.Pin.objects.count()

        data = self.create_input_data(
            NEW_DEVICE1_NAME, NEW_DEVICE1_IP, NEW_DEVICE1_PORT,
            pins=[(NEW_INPUT_PIN1_NAME, "input"),
                  (NEW_OUTPUT_PIN_NAME, "output")])
        device_service.update_device_and_pins(data)

        data = self.create_input_data(
            NEW_DEVICE2_NAME, NEW_DEVICE2_IP, NEW_DEVICE2_PORT,
            pins=[(NEW_INPUT_PIN2_NAME, "input"),
                  (NEW_OUTPUT_PIN_NAME, "output")])
        device_service.update_device_and_pins(data)

        final_device_count = models.Device.objects.count()
        final_pin_count = models.Pin.objects.count()

        self.assertEqual(initial_device_count + 2, final_device_count)
        self.assertEqual(initial_pin_count + 4, final_pin_count)
        new_device1 = models.Device.objects.get(name=NEW_DEVICE1_NAME)
        new_device2 = models.Device.objects.get(name=NEW_DEVICE2_NAME)
        added_pins = models.Pin.objects.filter(
            device__in=[new_device1, new_device2])
        expected_entries = set([
            (new_device1, NEW_INPUT_PIN1_NAME, models.Pin.Kind.INPUT.value),
            (new_device1, NEW_OUTPUT_PIN_NAME, models.Pin.Kind.OUTPUT.value),
            (new_device2, NEW_INPUT_PIN2_NAME, models.Pin.Kind.INPUT.value),
            (new_device2, NEW_OUTPUT_PIN_NAME, models.Pin.Kind.OUTPUT.value)])
        self.assertSetEqual(expected_entries, set(map(
            lambda e: (e.device, e.name, e.kind), added_pins)))

    def test_update_device_and_pins_reconfigure_pins(self):
        NEW_DEVICE_NAME = "NewDevice"
        NEW_DEVICE_IP = "192.168.1.10"
        NEW_DEVICE_PORT = 80
        OLD_INPUT_PIN_NAME = "OldInputPin"
        NEW_INPUT_PIN_NAME = "NewInputPin"
        NEW_OUTPUT_PIN_NAME = "NewOutputPin"

        data = self.create_input_data(
            NEW_DEVICE_NAME, NEW_DEVICE_IP, NEW_DEVICE_PORT,
            pins=[(OLD_INPUT_PIN_NAME, "input"),
                  (NEW_OUTPUT_PIN_NAME, "output")])

        device_service = services.DeviceService()
        device_service.update_device_and_pins(data)

        data = self.create_input_data(
            NEW_DEVICE_NAME, NEW_DEVICE_IP, NEW_DEVICE_PORT,
            pins=[(NEW_INPUT_PIN_NAME, "input"),
                  (NEW_OUTPUT_PIN_NAME, "output")])

        initial_device_count = models.Device.objects.count()
        initial_pin_count = models.Pin.objects.count()
        device_service.update_device_and_pins(data)
        final_device_count = models.Device.objects.count()
        final_pin_count = models.Pin.objects.count()

        self.assertEqual(initial_device_count, final_device_count)
        self.assertEqual(initial_pin_count, final_pin_count)
        added_device = models.Device.objects.get(name=NEW_DEVICE_NAME)
        added_pins = models.Pin.objects.filter(device=added_device)
        expected_entries = set([
            (NEW_INPUT_PIN_NAME, models.Pin.Kind.INPUT.value),
            (NEW_OUTPUT_PIN_NAME, models.Pin.Kind.OUTPUT.value)])
        self.assertSetEqual(expected_entries, set(map(
            lambda e: (e.name, e.kind), added_pins)))

    def test_update_device_and_pins_do_not_update_pins_on_event(self):
        NEW_DEVICE_NAME = "NewDevice"
        NEW_DEVICE_IP = "192.168.1.10"
        NEW_DEVICE_PORT = 80
        NEW_INPUT_PIN_NAME = "NewInputPin"
        NEW_OUTPUT_PIN_NAME = "NewOutputPin"

        data = self.create_input_data(
            NEW_DEVICE_NAME, NEW_DEVICE_IP, NEW_DEVICE_PORT,
            pins=[(NEW_INPUT_PIN_NAME, "input"),
                  (NEW_OUTPUT_PIN_NAME, "output")])

        device_service = services.DeviceService()
        device_service.update_device_and_pins(data)

        data = self.create_input_data(
            NEW_DEVICE_NAME, NEW_DEVICE_IP, NEW_DEVICE_PORT,
            input_type="event", pins=[(NEW_INPUT_PIN_NAME, "input")])

        initial_device_count = models.Device.objects.count()
        initial_pin_count = models.Pin.objects.count()
        device_service = services.DeviceService()
        device_service.update_device_and_pins(data)
        final_device_count = models.Device.objects.count()
        final_pin_count = models.Pin.objects.count()

        self.assertEqual(initial_device_count, final_device_count)
        self.assertEqual(initial_pin_count, final_pin_count)
        added_device = models.Device.objects.get(name=NEW_DEVICE_NAME)
        added_pins = models.Pin.objects.filter(device=added_device)
        expected_entries = set([
            (NEW_INPUT_PIN_NAME, models.Pin.Kind.INPUT.value),
            (NEW_OUTPUT_PIN_NAME, models.Pin.Kind.OUTPUT.value)])
        self.assertSetEqual(expected_entries, set(map(
            lambda e: (e.name, e.kind), added_pins)))


class TriggerServiceTest(TestCase):
    DEVICE1_NAME = "Device1"
    DEVICE2_NAME = "Device2"
    INPUT_PIN_NAME = "Pin1"
    OUTPUT_PIN_NAME = "Pin2"

    def setUp(self):
        self.device1 = models.Device.objects.create(
            name=self.DEVICE1_NAME, ip_address='1.2.3.4', port=8080,
            version=1)
        self.device2 = models.Device.objects.create(
            name=self.DEVICE2_NAME, ip_address='1.1.1.1', port=8080,
            version=1)
        self.input_pin_1 = models.Pin.objects.create(
            device=self.device1, name=self.INPUT_PIN_NAME,
            kind=models.Pin.Kind.INPUT.value, expression=None)
        self.input_pin_2 = models.Pin.objects.create(
            device=self.device2, name=self.INPUT_PIN_NAME,
            kind=models.Pin.Kind.INPUT.value, expression=None)
        self.output_pin_1 = models.Pin.objects.create(
            device=self.device1, name=self.OUTPUT_PIN_NAME,
            kind=models.Pin.Kind.OUTPUT.value, expression=None)
        self.output_pin_2 = models.Pin.objects.create(
            device=self.device2, name=self.OUTPUT_PIN_NAME,
            kind=models.Pin.Kind.OUTPUT.value, expression=None)

        self.mock_device_service = unittest.mock.Mock()
        self.mock_variable_proxy = unittest.mock.Mock()
        self.trigger_service = services.TriggerService(
            self.mock_device_service)
        self.trigger_service.VARIABLE_PROXY = self.mock_variable_proxy

    def test_process_triggers_trigger_to_the_correct_edge(self):
        rising_expression = models.Expression.objects.create(
            value='var.rising_trigger(pin.device, pin.pin, pin.value)')
        falling_expression = models.Expression.objects.create(
            value='var.falling_trigger(pin.device, pin.pin, pin.value)')
        both_expression = models.Expression.objects.create(
            value='var.both_trigger(pin.device, pin.pin, pin.value)')
        models.InputTrigger.objects.create(
            pin=self.input_pin_1, edge=models.InputTrigger.Edge.RISING.value,
            expression=rising_expression)
        models.InputTrigger.objects.create(
            pin=self.input_pin_1, edge=models.InputTrigger.Edge.FALLING.value,
            expression=falling_expression)
        models.InputTrigger.objects.create(
            pin=self.input_pin_1, edge=models.InputTrigger.Edge.BOTH.value,
            expression=both_expression)

        self.mock_device_service.get_intended_pin_states.return_value = {}

        self.trigger_service.process_triggers(self.input_pin_1, 1)
        self.trigger_service.process_triggers(self.input_pin_1, 0)

        self.mock_variable_proxy.rising_trigger.assert_has_calls([
            call(self.DEVICE1_NAME, self.INPUT_PIN_NAME, 1)])
        self.mock_variable_proxy.falling_trigger.assert_has_calls([
            call(self.DEVICE1_NAME, self.INPUT_PIN_NAME, 0)])
        self.mock_variable_proxy.both_trigger.assert_has_calls([
            call(self.DEVICE1_NAME, self.INPUT_PIN_NAME, 1),
            call(self.DEVICE1_NAME, self.INPUT_PIN_NAME, 0)])
        self.assertEqual(
            1, self.mock_variable_proxy.rising_trigger.call_count)
        self.assertEqual(
            1, self.mock_variable_proxy.falling_trigger.call_count)
        self.assertEqual(
            2, self.mock_variable_proxy.both_trigger.call_count)

    def test_process_triggers_trigger_to_the_correct_pin(self):
        expression = models.Expression.objects.create(
            value='var.trigger()')
        models.InputTrigger.objects.create(
            pin=self.input_pin_1, edge=models.InputTrigger.Edge.BOTH.value,
            expression=expression)

        self.mock_device_service.get_intended_pin_states.return_value = {}

        self.trigger_service.process_triggers(self.input_pin_2, 1)

        self.mock_variable_proxy.trigger.assert_not_called()

    def test_process_triggers_return_with_modified_pins(self):
        expression = models.Expression.objects.create(value='pass')
        models.InputTrigger.objects.create(
            pin=self.input_pin_1, edge=models.InputTrigger.Edge.BOTH.value,
            expression=expression)
        self.mock_device_service.get_intended_pin_states.side_effect = [{
            self.DEVICE1_NAME: {
                self.OUTPUT_PIN_NAME: 0},
            self.DEVICE2_NAME: {
                self.OUTPUT_PIN_NAME: 0}}, {
            self.DEVICE1_NAME: {
                self.OUTPUT_PIN_NAME: 0},
            self.DEVICE2_NAME: {
                self.OUTPUT_PIN_NAME: 1}}]

        result = self.trigger_service.process_triggers(self.input_pin_1, 1)

        expected_result = {self.DEVICE2_NAME: {self.OUTPUT_PIN_NAME: 1}}
        self.assertEqual(expected_result, result)
