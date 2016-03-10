from django.test import TestCase
from home.models import Variable


class VariableTest(TestCase):
    FOO_NAME = 'Foo'
    BAR_NAME = 'Bar'
    FOO_VALUE = 1
    BAR_VALUE = 3

    def setUp(self):
        Variable.objects.create(name=self.FOO_NAME, value=self.FOO_VALUE)
        Variable.objects.create(name=self.BAR_NAME, value=self.BAR_VALUE)

    def tearDown(self):
        Variable.objects.all().delete()

    def test_get_value_of_variable(self):
        foo = Variable.objects.get(name=self.FOO_NAME)
        bar = Variable.objects.get(name=self.BAR_NAME)
        self.assertEqual(self.FOO_VALUE, foo.get())
        self.assertEqual(self.BAR_VALUE, bar.get())

    def test_set_value_of_variable(self):
        FOO_NEW_VALUE = 5
        foo = Variable.objects.get(name=self.FOO_NAME)
        foo.set(FOO_NEW_VALUE)
        foo.save()
        foo = None
        foo = Variable.objects.get(name=self.FOO_NAME)
        self.assertEqual(FOO_NEW_VALUE, foo.get())
        foo.set(self.FOO_VALUE)
        foo.save()

    def test_toggle_value_of_variable(self):
        foo = Variable.objects.get(name=self.FOO_NAME)
        foo.toggle()
        self.assertEqual(0, foo.get())

    def test_toggle_value_of_variable_with_modulo(self):
        foo = Variable.objects.get(name=self.FOO_NAME)
        foo.toggle(3)
        self.assertEqual(2, foo.get())
