// Settings page (Clay) with Monokuma's Seeds of Despair under the title; the
// counter and the page handling are shared: shared/src/pkjs/minute_counter.js.
// One seed per 4 hours Monokuma is the active face, up to 4 a day: worn
// every day, about 3 months to 365.
require('../../../../shared/src/pkjs/minute_counter').start({
  Clay: require('@rebble/clay'),
  clayConfig: require('./config'),
  keys: require('message_keys'),
  image: require('./page_image'),
  title: 'Seeds of Despair Planted',
  nextIn: 'Next seed in',
  doneToday: 'Today’s seeds planted ✓',
  goal: 365,
  unitMinutes: 4 * 60,
  perDay: 4
});
