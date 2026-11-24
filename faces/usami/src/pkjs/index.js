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
var STORAGE = 'hope-fragments';
var WAIT_MS = 2000;         // how long to wait for the watch before opening
var FOLD_MINUTES = 90 * 24 * 60;  // spans older than this become a number

var pendingOpen = null;     // timer while the settings page waits

// Hope Fragments: the clock minutes (UTC minutes since 1970) in which any
// watch ran the face, each counted once. Watches send the spans of minutes
// they ran ([start, end) pairs; see shared/src/c/minute_counter.c), and
// they're merged here, so two watches running at once count once.
//   spans: merged, sorted [start, end) pairs
//   base:  minutes in spans older than FOLD_MINUTES, folded into a number
//   floor: the highest lifetime total any watch has reported; the count is
//          never lower (if this data was lost, or a watch dropped spans)
function loadState() {
  var st = null;
  try { st = JSON.parse(localStorage.getItem(STORAGE)); } catch (e) {}
  st = st || {};
  return { spans: st.spans || [], base: st.base || 0, floor: st.floor || 0 };
}

function saveState(st) {
  localStorage.setItem(STORAGE, JSON.stringify(st));
}

function addSpans(spans, add) {
  var all = spans.concat(add).sort(function(a, b) { return a[0] - b[0]; });
  var out = [];
  all.forEach(function(s) {
    var last = out[out.length - 1];
    if (last && s[0] <= last[1]) {
      last[1] = Math.max(last[1], s[1]);
    } else {
      out.push([s[0], s[1]]);
    }
  });
  return out;
}

function fragmentCount() {
  var st = loadState();
  var n = st.base;
  st.spans.forEach(function(s) { n += s[1] - s[0]; });
  return Math.max(n, st.floor);
}

// Ask the watch for its spans. ackId is the last span id of the batch just
// stored (the watch then forgets those), or 0 to just ask.
function requestSync(ackId, onFail) {
  var msg = {};
  msg[keys.MINUTE_SYNC] = ackId;
  Pebble.sendAppMessage(msg, null, onFail);
}

function payloadValue(payload, name) {
  var v = payload[keys[name]];
  return v === undefined ? payload[name] : v;
}

function readUint32(bytes, i) {
  return (bytes[i] | (bytes[i + 1] << 8) | (bytes[i + 2] << 16)) +
    bytes[i + 3] * 16777216;
}

// Store a batch of spans from the watch. Returns the id of its last closed
// span to acknowledge, or 0 if there's none (only the growing one).
function receiveSpans(payload) {
  var firstId = payloadValue(payload, 'MINUTE_FIRST_ID');
  var bytes = payloadValue(payload, 'MINUTE_SPANS') || [];
  var open = payloadValue(payload, 'MINUTE_OPEN');
  var total = payloadValue(payload, 'MINUTE_TOTAL') || 0;
  var add = [];
  for (var i = 0; i + 8 <= bytes.length; i += 8) {
    var s = readUint32(bytes, i), e = readUint32(bytes, i + 4);
    if (e > s) { add.push([s, e]); }
  }
  var st = loadState();
  st.spans = addSpans(st.spans, add);
  st.floor = Math.max(st.floor, total);
  // Fold old spans into base so the list stays short.
  var cutoff = Math.floor(Date.now() / 60000) - FOLD_MINUTES;
  while (st.spans.length && st.spans[0][1] < cutoff) {
    var old = st.spans.shift();
    st.base += old[1] - old[0];
  }
  saveState(st);
  var closed = bytes.length / 8 - (open ? 1 : 0);
  return closed > 0 ? firstId + closed - 1 : 0;
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
    if (item.id === 'hope') { item.defaultValue = hopeHtml(fragmentCount()); }
  });
  Pebble.openURL(clay.generateUrl());
}

Pebble.addEventListener('ready', function() {
  requestSync(0);
});

Pebble.addEventListener('appmessage', function(e) {
  if (payloadValue(e.payload, 'MINUTE_FIRST_ID') === undefined) { return; }
  var ackId = receiveSpans(e.payload);
  if (pendingOpen) { openSettings(); }
  // Acknowledge the closed spans; the watch then forgets them and sends the
  // rest.
  if (ackId) { requestSync(ackId); }
});

Pebble.addEventListener('showConfiguration', function() {
  // Get the latest spans first; open with what's stored if the watch doesn't
  // answer in time (or the face isn't running).
  pendingOpen = setTimeout(openSettings, WAIT_MS);
  requestSync(0, function() { if (pendingOpen) { openSettings(); } });
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) { return; }
  Pebble.sendAppMessage(clay.getSettings(e.response), null, function(error) {
    console.log('Failed to send settings: ' + JSON.stringify(error));
  });
});
