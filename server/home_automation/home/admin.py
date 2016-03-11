from .models import Expression, Device, Pin, Variable, InputTrigger, Log

from django import forms
from django.contrib import admin

class PinForm(forms.ModelForm):
    kind = forms.ChoiceField(choices=Pin.Kind.choices())
    class Meta:
        model = Pin
        exclude = ['kind']

class PinAdmin(admin.ModelAdmin):
    readonly_fields = ('kind',)
    form = PinForm
    def formfield_for_dbfield(self, db_field, **kwargs):
        if db_field.name == 'kind':
            kwargs['kind'].choices = Pin.Kind.choices()
        return super(PinAdmin, self).formfield_for_dbfield(db_field, **kwargs)

admin.site.register(Expression)
admin.site.register(Device)
admin.site.register(Pin, PinAdmin)
admin.site.register(Variable)
admin.site.register(InputTrigger)
admin.site.register(Log)
