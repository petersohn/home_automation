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
  $("#status").load("/ajax/status.html", function() {
  })
}

function loadDevices(event, tab) {
  loadTabData($("#devices"), "/ajax/devices.html", tab);
}

function loadLogs(event, tab) {
  loadTabData($("#logs"), "/ajax/logs.html", tab);
  //$.get("/ajax/Logs.json",
    //function(data) {
      //var items = new vis.DataSet(JSON.parse(data))
      //createTimeline()
    //})
}

function createTimeline(data) {
  $("#logs").empty()

  var container = $("#logs").get(0)
  var options = {}

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

  $("#admin-frame").height($(window).height() - 20)

  setInterval(loadStatus, 10000)
});

