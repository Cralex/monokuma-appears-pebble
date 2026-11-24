var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var keys = require('message_keys');
var gemImage = require('./gem_image');

// Clay builds the settings page and stores its values in localStorage; this
// file opens the page and sends the values to the watch (SHAKE_ENABLED as
// 1/0) when the user saves. It handles those events itself so it can first
// put the watch's minute count ("Hope Fragments Collected") on the page.
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var HOPE_GOAL = 365;        // shown as count/365, never more than 365
var COUNT_STORAGE = 'minute-count';
var WAIT_MS = 2000;         // how long to wait for the watch's count

var pendingOpen = null;     // timer while the settings page waits

function storedCount() {
  var n = parseInt(localStorage.getItem(COUNT_STORAGE), 10);
  return isNaN(n) ? 0 : n;
}

// The watch answers any MINUTE_COUNT message with its count.
function requestCount(onFail) {
  var msg = {};
  msg[keys.MINUTE_COUNT] = 0;
  Pebble.sendAppMessage(msg, null, onFail);
}

function hopeHtml(count) {
  var shown = Math.min(count, HOPE_GOAL);
  return '<span style="display:block;text-align:center">' +
    '<img src="' + gemImage + '" width="52" height="52" alt="" ' +
    'style="display:block;margin:0 auto 6px">' +
    '<span style="display:block">Hope Fragments Collected</span>' +
    '<span style="display:block;font-size:2em;font-weight:bold">' +
    shown + '/' + HOPE_GOAL + '</span></span>';
}

function openSettings() {
  if (pendingOpen) { clearTimeout(pendingOpen); pendingOpen = null; }
  clay.config.forEach(function(item) {
    if (item.id === 'hope') { item.defaultValue = hopeHtml(storedCount()); }
  });
  Pebble.openURL(clay.generateUrl());
}

Pebble.addEventListener('ready', function() {
  requestCount();
});

Pebble.addEventListener('appmessage', function(e) {
  var count = e.payload[keys.MINUTE_COUNT];
  if (count === undefined) { count = e.payload.MINUTE_COUNT; }
  if (count === undefined) { return; }
  localStorage.setItem(COUNT_STORAGE, String(count));
  if (pendingOpen) { openSettings(); }
});

Pebble.addEventListener('showConfiguration', function() {
  // Ask for the latest count; open with the last one received if the watch
  // doesn't answer in time (or the face isn't running).
  pendingOpen = setTimeout(openSettings, WAIT_MS);
  requestCount(function() { if (pendingOpen) { openSettings(); } });
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) { return; }
  Pebble.sendAppMessage(clay.getSettings(e.response), null, function(error) {
    console.log('Failed to send settings: ' + JSON.stringify(error));
  });
});
