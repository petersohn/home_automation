function loadStatus() {
  $("#status").load("/ajax/Status.html", function() {
  })
}

function loadDevices() {
  $("#devices").load("/ajax/Devices.html");
}

function loadLogs() {
  $("#logs").load("/ajax/Logs.html");
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
  loadDevices()

  $("#menu").tabs({beforeActivate: function(event, ui) {
    ui.newPanel.trigger("loadContent")
  }})

  $("#devices").bind("loadContent", loadDevices)
  $("#logs").bind("loadContent", loadLogs)

  $("#admin-frame").height($(window).height() - 20)

  setInterval(loadStatus, 10000)
});

