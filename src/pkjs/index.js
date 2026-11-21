var SHAKE_KEY = 'shake_enabled';

function sendShakeSetting(enabled) {
  Pebble.sendAppMessage(
    { 'SHAKE_ENABLED': enabled ? 1 : 0 },
    function() { console.log('Shake setting sent: ' + enabled); },
    function(e) { console.log('Send failed: ' + JSON.stringify(e)); }
  );
}

function getShakeSetting() {
  // Default true: any stored value other than 'false' means enabled
  return localStorage.getItem(SHAKE_KEY) !== 'false';
}

Pebble.addEventListener('ready', function() {
  sendShakeSetting(getShakeSetting());
});

Pebble.addEventListener('showConfiguration', function() {
  var shake = getShakeSetting();

  var html =
    '<!DOCTYPE html><html><head>' +
    '<meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<style>' +
    'body{font-family:sans-serif;padding:24px;background:#1c1c1c;color:#f0f0f0;margin:0}' +
    'h2{margin:0 0 24px;font-size:22px}' +
    'label{display:flex;align-items:center;gap:14px;font-size:18px;cursor:pointer}' +
    'input[type=checkbox]{width:28px;height:28px;accent-color:#c0392b;cursor:pointer}' +
    'button{display:block;width:100%;margin-top:32px;padding:14px;font-size:16px;' +
    'background:#c0392b;color:#fff;border:none;border-radius:8px;cursor:pointer}' +
    '</style></head><body>' +
    '<h2>Monokuma Appears</h2>' +
    '<label>' +
    '<input type="checkbox" id="s"' + (shake ? ' checked' : '') + '>' +
    'Change image on shake' +
    '</label>' +
    '<button onclick="save()">Save</button>' +
    '<script>' +
    'function save(){' +
    'var r={shake:document.getElementById("s").checked};' +
    'location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify(r));' +
    '}' +
    '<\/script></body></html>';

  Pebble.openURL('data:text/html,' + encodeURIComponent(html));
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response || e.response === 'CANCELLED') return;
  try {
    var config = JSON.parse(decodeURIComponent(e.response));
    var enabled = config.shake !== false;
    localStorage.setItem(SHAKE_KEY, enabled ? 'true' : 'false');
    sendShakeSetting(enabled);
  } catch (err) {
    console.log('Config parse error: ' + err);
  }
});
