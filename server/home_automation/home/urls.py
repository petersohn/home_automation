from django.conf.urls import url
from home.views import StatusView


urlpatterns = [
    url(r'^device/status(/?)', StatusView.as_view(), name='status'),
]
