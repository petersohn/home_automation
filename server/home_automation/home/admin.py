from .models import Expression, Device, Pin, Variable, InputTrigger, Log

from django import forms
from django.contrib import admin


class PinAdmin(admin.ModelAdmin):
    readonly_fields = ('kind',)


admin.site.register(Expression)
admin.site.register(Device)
admin.site.register(Pin, PinAdmin)
admin.site.register(Variable)
admin.site.register(InputTrigger)
admin.site.register(Log)
