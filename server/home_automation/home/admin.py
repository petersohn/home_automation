from django.contrib import admin
from .models import Expression, Device, Pin, Variable, InputTrigger, Log

admin.site.register(Expression)
admin.site.register(Device)
admin.site.register(Pin)
admin.site.register(Variable)
admin.site.register(InputTrigger)
admin.site.register(Log)
