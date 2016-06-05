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

$("#devices_link").parent().addClass("active")

$(document).ready(function() {
  loadStatus()
  loadDevices()

  $("#menu").tabs()

  $("#devices-header").click(function() {
    loadDevices()
  })

  $("#logs-header").click(function() {
    loadLogs()
  })

  $("#admin-frame").height($(window).height() - 20)

  setInterval(loadStatus, 10000)
});

