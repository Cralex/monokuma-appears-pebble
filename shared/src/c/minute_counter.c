// Minute counter: records which clock minutes the face ran, so the phone can
// count each minute once however many watches ran it.
//
// Every minute tick counts that clock minute (UTC minutes since 1970). Runs
// of consecutive minutes are kept as spans [start, end), each with an id
// (consecutive, from 1). The phone merges the spans from all watches and
// counts the minutes in their union, so two watches running at the same time
// count once, and a watch that ran without the phone counts in full when it
// next syncs. The watch also keeps its own lifetime total, which the phone
// uses as a floor (if the phone's data is lost, or spans were dropped).
//
// Sync: the phone sends MINUTE_SYNC = the last span id of the batch it just
// stored (0 to just ask). The watch forgets the closed spans up to that id
// (ids keep counting up, so an old id can't drop new spans) and replies
// with up to SEND_MAX spans from the oldest: MINUTE_FIRST_ID (the first one's
// id), MINUTE_SPANS (start/end pairs, little-endian uint32), MINUTE_OPEN (1 if
// the last one is still growing) and MINUTE_TOTAL. The phone acknowledges the
// closed ones with another MINUTE_SYNC, and so on until only the open span is
// left. Sending a span twice is harmless: the phone merges, never adds.
//
// Everything is kept in RAM and saved every SAVE_MINUTES and when the face
// closes, so a crash loses at most a few minutes.

#include "minute_counter.h"
#include "face_config.h"

#ifndef MINUTE_COUNTER
#define MINUTE_COUNTER 0
#endif

#if MINUTE_COUNTER

#define PERSIST_KEY_TOTAL    2   // the lifetime total (was the plain count)
#define PERSIST_KEY_FIRST_ID 3
#define PERSIST_KEY_NUM      4
#define PERSIST_KEY_SPANS    5   // 5 and 6, SPANS_PER_KEY each
#define SPANS_PER_KEY        32  // 32 * 8 bytes = PERSIST_DATA_MAX_LENGTH
#define MAX_SPANS            64
#define SEND_MAX             32
#define SAVE_MINUTES         10

typedef struct {
    uint32_t start;  // first minute
    uint32_t end;    // one past the last minute
} Span;

static Span     s_spans[MAX_SPANS];
static int      s_num        = 0;
static uint32_t s_first_id   = 1;      // id of s_spans[0]
static bool     s_open       = false;  // the last span is this session's
static int32_t  s_total      = 0;
static int      s_since_save = 0;

void minute_counter_load(void) {
    s_total = persist_read_int(PERSIST_KEY_TOTAL);  // 0 if never saved
    if (persist_exists(PERSIST_KEY_FIRST_ID)) {
        s_first_id = (uint32_t)persist_read_int(PERSIST_KEY_FIRST_ID);
    }
    s_num = persist_read_int(PERSIST_KEY_NUM);
    if (s_num < 0 || s_num > MAX_SPANS) { s_num = 0; }
    for (int k = 0; k * SPANS_PER_KEY < s_num; k++) {
        persist_read_data(PERSIST_KEY_SPANS + k, &s_spans[k * SPANS_PER_KEY],
                          SPANS_PER_KEY * sizeof(Span));
    }
    s_open = false;  // a new session starts a new span
}

void minute_counter_save(void) {
    persist_write_int(PERSIST_KEY_TOTAL, s_total);
    persist_write_int(PERSIST_KEY_FIRST_ID, (int32_t)s_first_id);
    persist_write_int(PERSIST_KEY_NUM, s_num);
    for (int k = 0; k * SPANS_PER_KEY < s_num; k++) {
        int n = s_num - k * SPANS_PER_KEY;
        if (n > SPANS_PER_KEY) { n = SPANS_PER_KEY; }
        persist_write_data(PERSIST_KEY_SPANS + k, &s_spans[k * SPANS_PER_KEY],
                           n * sizeof(Span));
    }
    s_since_save = 0;
}

static void drop_first(int n) {
    memmove(&s_spans[0], &s_spans[n], (s_num - n) * sizeof(Span));
    s_num -= n;
    s_first_id += n;
}

void minute_counter_tick(void) {
    uint32_t m = (uint32_t)(time(NULL) / 60);
    Span *last = s_num ? &s_spans[s_num - 1] : NULL;
    if (s_open && m == last->end) {
        last->end++;                     // the next minute of this run
    } else if (s_open && m >= last->start && m < last->end) {
        return;                          // already counted
    } else {
        if (s_num == MAX_SPANS) {
            // Long without the phone: forget the oldest run. The total
            // still counts it.
            drop_first(1);
        }
        s_spans[s_num++] = (Span){ m, m + 1 };
        s_open = true;
    }
    if (s_total < INT32_MAX) { s_total++; }
    if (++s_since_save >= SAVE_MINUTES) { minute_counter_save(); }
}

static void send_spans(void) {
    DictionaryIterator *out;
    if (app_message_outbox_begin(&out) != APP_MSG_OK) { return; }
    int n = s_num < SEND_MAX ? s_num : SEND_MAX;
    uint8_t buf[SEND_MAX * 8];
    for (int i = 0; i < n; i++) {
        uint32_t v[2] = { s_spans[i].start, s_spans[i].end };
        for (int j = 0; j < 8; j++) {
            buf[i * 8 + j] = (uint8_t)(v[j / 4] >> (8 * (j % 4)));
        }
    }
    dict_write_int32(out, MESSAGE_KEY_MINUTE_FIRST_ID, (int32_t)s_first_id);
    dict_write_data(out, MESSAGE_KEY_MINUTE_SPANS, buf, n * 8);
    dict_write_int32(out, MESSAGE_KEY_MINUTE_OPEN,
                     (s_open && n == s_num) ? 1 : 0);
    dict_write_int32(out, MESSAGE_KEY_MINUTE_TOTAL, s_total);
    app_message_outbox_send();
}

void minute_counter_inbox(DictionaryIterator *iter) {
    Tuple *t = dict_find(iter, MESSAGE_KEY_MINUTE_SYNC);
    if (!t) { return; }
    // Forget the closed spans the phone has stored.
    uint32_t acked = (uint32_t)t->value->int32;
    int closed = s_open ? s_num - 1 : s_num;
    int n = 0;
    while (n < closed && s_first_id + n <= acked) { n++; }
    if (n > 0) {
        drop_first(n);
        minute_counter_save();
    }
    send_spans();
}

#else

void minute_counter_load(void) {}
void minute_counter_save(void) {}
void minute_counter_tick(void) {}
void minute_counter_inbox(DictionaryIterator *iter) {}

#endif
