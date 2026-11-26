// Phone side of the minute counter (watch side: shared/src/c/minute_counter.c)
// and the settings page that shows it. A face's src/pkjs/index.js calls
// start() with its Clay settings and the counter's name and goal; see
// faces/usami/src/pkjs/index.js.
//
// The face earns one unit (a Hope Fragment, a Seed of Despair) for every
// unitMinutes it's the active face, at most perDay a day (days in the
// phone's time zone), shown under the settings page's title as count/goal
// with one line under it: the time to the next unit, or doneToday once the
// day's are earned.
//
// The tally comes from the clock minutes (UTC minutes since 1970) in which
// any watch ran the face, each counted once. Watches send the spans of
// minutes they ran ([start, end) pairs), and they're merged here, so two
// watches running at once count once, and a watch used away from the phone
// counts in full when it next syncs. Stored in localStorage:
//   spans: merged, sorted [start, end) pairs
//   days:  minutes per local day ("YYYY-MM-DD") from spans older than
//          FOLD_MINUTES, folded so the span list stays short
//   floor: the highest lifetime total (minutes) any watch has reported; the
//          count never drops below what that alone would earn (if this data
//          was lost, or a watch dropped spans)

var STORAGE = 'minute-counter';
var WAIT_MS = 2000;               // how long to wait for the watch
var FOLD_MINUTES = 30 * 24 * 60;  // spans older than this become day totals

// opts:
//   Clay, clayConfig  the Clay class and the settings config; the config
//                     needs a { "type": "text", "id": "counter" } item,
//                     which gets the counter
//   keys              require('message_keys')
//   image             { uri, width, height } (CSS px), from page_image.js
//   title             e.g. 'Hope Fragments Collected'
//   nextIn            e.g. 'Next fragment in' (followed by '2 h 15 min')
//   doneToday         e.g. 'Today’s fragments collected ✓'
//   goal, unitMinutes, perDay
exports.start = function(opts) {
  // Clay builds the settings page and stores its values in localStorage;
  // this opens the page (after putting the counter on it) and sends the
  // values to the watch when the user saves.
  var clay = new opts.Clay(opts.clayConfig, null, { autoHandleEvents: false });
  var keys = opts.keys;
  var pendingOpen = null;  // timer while the settings page waits

  function loadState() {
    var st = null;
    try { st = JSON.parse(localStorage.getItem(STORAGE)); } catch (e) {}
    st = st || {};
    return { spans: st.spans || [], days: st.days || {}, floor: st.floor || 0 };
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

  function dayKey(date) {
    function two(n) { return (n < 10 ? '0' : '') + n; }
    return date.getFullYear() + '-' + two(date.getMonth() + 1) + '-' +
      two(date.getDate());
  }

  // Add the minutes of spans to days (minutes per local day), split at
  // midnight.
  function addToDays(days, spans) {
    spans.forEach(function(span) {
      var m = span[0];
      while (m < span[1]) {
        var d = new Date(m * 60000);
        var midnight = new Date(d.getFullYear(), d.getMonth(),
                                d.getDate() + 1).getTime() / 60000;
        var n = Math.min(span[1], midnight) - m;
        var key = dayKey(d);
        days[key] = (days[key] || 0) + n;
        m += n;
      }
    });
  }

  // Go through the days in order: minutes add up (carrying over from day to
  // day) and every unitMinutes earns a unit, up to perDay; a day's minutes
  // past its last unit are dropped.
  //   count:    units earned
  //   today:    units earned today
  //   progress: minutes toward the next one
  function tally() {
    var st = loadState();
    var days = {};
    for (var k in st.days) { days[k] = st.days[k]; }
    addToDays(days, st.spans);
    var today = dayKey(new Date());
    var t = { count: 0, today: 0, progress: 0 };
    Object.keys(days).sort().forEach(function(key) {
      t.progress += days[key];
      var earned = Math.min(opts.perDay,
                            Math.floor(t.progress / opts.unitMinutes));
      t.progress -= earned * opts.unitMinutes;
      if (earned === opts.perDay) { t.progress = 0; }
      t.count += earned;
      if (key === today) { t.today = earned; }
    });
    // At most perDay per 24 hours of a watch's own minutes.
    var rate = Math.min(1 / opts.unitMinutes, opts.perDay / (24 * 60));
    t.count = Math.max(t.count, Math.floor(st.floor * rate));
    return t;
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
    // Fold old spans into day totals so the list stays short.
    var cutoff = Math.floor(Date.now() / 60000) - FOLD_MINUTES;
    var old = [];
    while (st.spans.length && st.spans[0][1] < cutoff) {
      old.push(st.spans.shift());
    }
    addToDays(st.days, old);
    saveState(st);
    var closed = bytes.length / 8 - (open ? 1 : 0);
    return closed > 0 ? firstId + closed - 1 : 0;
  }

  function duration(minutes) {
    var h = Math.floor(minutes / 60), m = minutes % 60;
    if (!h) { return m + ' min'; }
    return m ? h + ' h ' + m + ' min' : h + ' h';
  }

  function counterHtml(t) {
    var shown = Math.min(t.count, opts.goal);
    var note = '';
    if (shown < opts.goal) {
      note = t.today >= opts.perDay
        ? opts.doneToday
        : opts.nextIn + ' ' + duration(opts.unitMinutes - t.progress);
    }
    var img = opts.image;
    return '<span style="display:block;text-align:center">' +
      '<img src="' + img.uri + '" width="' + img.width + '" height="' +
      img.height + '" alt="" style="display:block;margin:0 auto 6px">' +
      '<span style="display:block">' + opts.title + '</span>' +
      '<span style="display:block;font-size:2em;font-weight:bold">' +
      shown + '/' + opts.goal + '</span>' +
      (note ? '<span style="display:block;opacity:0.7">' + note + '</span>'
            : '') +
      '</span>';
  }

  function openSettings() {
    if (pendingOpen) { clearTimeout(pendingOpen); pendingOpen = null; }
    clay.config.forEach(function(item) {
      if (item.id === 'counter') { item.defaultValue = counterHtml(tally()); }
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
    // Acknowledge the closed spans; the watch then forgets them and sends
    // the rest.
    if (ackId) { requestSync(ackId); }
  });

  Pebble.addEventListener('showConfiguration', function() {
    // Get the latest spans first; open with what's stored if the watch
    // doesn't answer in time (or the face isn't running).
    pendingOpen = setTimeout(openSettings, WAIT_MS);
    requestSync(0, function() { if (pendingOpen) { openSettings(); } });
  });

  Pebble.addEventListener('webviewclosed', function(e) {
    if (!e || !e.response) { return; }
    Pebble.sendAppMessage(clay.getSettings(e.response), null, function(error) {
      console.log('Failed to send settings: ' + JSON.stringify(error));
    });
  });
};
