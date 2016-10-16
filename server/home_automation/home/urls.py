from django.conf.urls import url
from home import views


urlpatterns = [
    url(r'^device/status(/?)', views.StatusView.as_view(), name='status'),
    url(r'^$', views.IndexView.as_view(), name='Devices'),
    url(r'^home.js$', views.JavascriptView.as_view(), name='Devices'),
    url(r'api/', views.AjaxView.as_view(), name='Devices'),
    url(r'^device/status', views.StatusView.as_view(), name='status'),
]
