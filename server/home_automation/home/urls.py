from django.conf.urls import url
from . import views


urlpatterns = [
    url(r'^device/status', views.status, name='status'),
]
