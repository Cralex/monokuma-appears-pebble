var Clay = require('pebble-clay');
var clayConfig = require('./config');

// Clay builds the settings page, stores values in localStorage,
// and sends them to the watch (SHAKE_ENABLED as 1/0) when the user saves.
var clay = new Clay(clayConfig);
