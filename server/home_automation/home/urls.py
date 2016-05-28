from django.conf.urls import url
from home.views import AjaxView, IndexView, StatusView


urlpatterns = [
    url(r'^device/status(/?)', StatusView.as_view(), name='status'),
    url(r'^$', IndexView.as_view(), name='Devices'),
    url(r'ajax/', AjaxView.as_view(), name='Devices'),
    url(r'^device/status', StatusView.as_view(), name='status'),
]
