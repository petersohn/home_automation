function loadStatus() {
  $("#status").load("/ajax/Status.html")
}

function loadDevices() {
  $("#devices").load("/ajax/Devices.html")
}

function loadLogs() {
  $.get("/ajax/Logs.json",
    function(data) {
      var items = new vis.DataSet(JSON.parse(data))
      logsTimeline.setItems(items)
    })
}

function createTimeline() {
  var container = $("#logs").get(0)
  var items = []
  var options = {}

  logsTimeline = new vis.Timeline(container, items, options)
}

$("#devices_link").click(function() {
  $("#devices").show(0)
  $("#logs").hide(0)
  loadStatus()
  loadDevices()
})
$("#logs_link").click(function() {
  $("#devices").hide(0)
  $("#logs").show(0)
  loadStatus()
  loadLogs()
})

var logsTimeline

$("#logs").hide(0)
$("#devices").show(0)
$("#devices_link").parent().addClass("active")

$(document).ready(function() {
  $(".navbar-nav li a").click(function(event) { //Click on a navigation item
    $(".navbar-nav li").removeClass("active");   //Remove Active style of item
    $(event.target).parent().addClass("active")
  });

  createTimeline()
  loadStatus()
  loadDevices()

  setInterval(loadStatus, 10000)
});

