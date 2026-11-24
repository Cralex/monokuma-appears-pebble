#pragma once

// Optional minute counter. A face turns it on with #define MINUTE_COUNTER 1
// in face_config.h and the message keys MINUTE_SYNC, MINUTE_FIRST_ID,
// MINUTE_SPANS, MINUTE_OPEN and MINUTE_TOTAL in package.json; see
// minute_counter.c. Without it these do nothing.

#include <pebble.h>

void minute_counter_load(void);
void minute_counter_save(void);
// Call on every minute tick.
void minute_counter_tick(void);
// Call from the AppMessage inbox handler.
void minute_counter_inbox(DictionaryIterator *iter);
