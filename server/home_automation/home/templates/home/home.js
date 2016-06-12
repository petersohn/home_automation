{% load static %}
function loadTabData(target, url, tab) {
  target.load(url, function(response, status, xhr) {
    icon = $(tab).find("#warning");
    if (status == "success" || status == "notmodified") {
      icon.removeClass("ui-icon ui-icon-alert");
    } else {
      icon.addClass("ui-icon ui-icon-alert");
    }
  });
}

function loadStatus() {
  loadTabData($("#status-content"), "/ajax/status.html", $("#status"));
}

function loadDevices(event, tab) {
  loadTabData($("#devices"), "/ajax/devices.html", tab);
}

function loadLogs(event, tab) {
  loadTabData($("#logs"), "/ajax/logs.html", tab);
}

function loadLogTimeline(event, tab) {
  $.get("/ajax/logs.json",
    function(data) {
      var items = new vis.DataSet(data);
      createTimeline(items);
    });
}

function createTimeline(data) {
  panel = $("#log-timeline");
  panel.empty();

  var container = panel.get(0);
  var options = {
    margin: { item: { vertical: 4 }}
  }

  var logsTimeline = new vis.Timeline(container, data, options)
}

$(document).ready(function() {
  loadStatus()
  loadDevices(undefined, $("#devices").parent())

  $("#menu").tabs({beforeActivate: function(event, ui) {
    ui.newPanel.trigger("loadContent", ui.newTab)
  }})

  $("#devices").on("loadContent", loadDevices)
  $("#logs").on("loadContent", loadLogs)
  $("#log-timeline").on("loadContent", loadLogTimeline)

  $("#admin-frame").height($(window).height() - 20)

  setInterval(loadStatus, 10000)
});

