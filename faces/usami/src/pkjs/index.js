// Settings page (Clay) with Usami's Hope Fragments under the title; the
// counter and the page handling are shared: shared/src/pkjs/minute_counter.js.
// One fragment per 4 hours Usami is the active face, up to 4 a day: worn
// every day, about 3 months to 365.
require('../../../../shared/src/pkjs/minute_counter').start({
  Clay: require('@rebble/clay'),
  clayConfig: require('./config'),
  keys: require('message_keys'),
  image: require('./page_image'),
  title: 'Hope Fragments Collected',
  nextIn: 'Next fragment in',
  doneToday: 'Today’s fragments collected ✓',
  goal: 365,
  unitMinutes: 4 * 60,
  perDay: 4
});
