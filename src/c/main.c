#include <pebble.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define NUM_FG_IMAGES     16

// Per-platform layout, scaled from emery's. The foreground keeps emery's
// 200:172 shape and sits on the bottom edge; the clock is centered in the
// band above it. Keep FG sizes in sync with tools/convert_images.py.
#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE) || \
    defined(PBL_PLATFORM_DIORITE)       // 144x168 (aplite/diorite: B&W)
#define FG_X              0
#define FG_W              144
#define FG_H              124
#define DORBLUE_RES       RESOURCE_ID_FONT_DORBLUE_34
#define DORBLUE_Y         -1
#define LECO_KEY          FONT_KEY_LECO_32_BOLD_NUMBERS
#define LECO_Y            2
#define SLIDE_STEP        9
#elif defined(PBL_PLATFORM_CHALK)       // 180x180 round
#define FG_X              19
#define FG_W              142
#define FG_H              123
#define DORBLUE_RES       RESOURCE_ID_FONT_DORBLUE_32
#define DORBLUE_Y         15
#define LECO_KEY          FONT_KEY_LECO_32_BOLD_NUMBERS
#define LECO_Y            16
#define SLIDE_STEP        9
#elif defined(PBL_PLATFORM_GABBRO)      // 260x260 round
#define FG_X              24
#define FG_W              212
#define FG_H              182
#define DORBLUE_RES       RESOURCE_ID_FONT_DORBLUE_46
#define DORBLUE_Y         20
#define LECO_KEY          FONT_KEY_LECO_42_NUMBERS
#define LECO_Y            24
#define SLIDE_STEP        13
#else                                   // emery, 200x228
#define FG_X              0
#define FG_W              200
#define FG_H              172
#define DORBLUE_RES       RESOURCE_ID_FONT_DORBLUE_48
#define DORBLUE_Y         -5    // nudges DorBlue to center it in the clock band
#define LECO_KEY          FONT_KEY_LECO_42_NUMBERS
#define LECO_Y            0
#define SLIDE_STEP        12    // pixels per frame
#endif

#define SCREEN_H          PBL_DISPLAY_HEIGHT
#define FG_Y_SHOWN        (SCREEN_H - FG_H)
#define FG_Y_HIDDEN       SCREEN_H
#define CLOCK_H           FG_Y_SHOWN
#define ANIM_INTERVAL_MS  33    // ~30 fps
#define FADE_MAX          4     // fade steps; 4 * 33ms = 132ms per fade
#define PAUSE_DURATION_MS 1000  // background-only pause between fg changes
#define INITIAL_DELAY_MIN_MS 1000  // random boot delay before first fade/clock
#define INITIAL_DELAY_MAX_MS 3000
#define PERSIST_KEY_SHAKE 0
#define PERSIST_KEY_FONT  1

// Debug: show every image in order, one every DEBUG_CYCLE_MS once it's fully
// shown, with the image number (01-16) in place of the time. Set to 0 for
// normal behavior (random images that change each minute).
#define DEBUG_CYCLE       0
#define DEBUG_CYCLE_MS    3000

// ============================================================================
// STATE
// ============================================================================

typedef enum {
    STATE_INIT,
    STATE_FADING_OUT,
    STATE_SLIDING_UP,
    STATE_SLIDING_DOWN,
    STATE_FADING_IN,
    STATE_PAUSING,
    STATE_SHOWN,
} WatchState;

// What to do when the current fade completes
typedef enum {
    AFTER_NONE,
    AFTER_SLIDE_UP,    // fade-out → select new fg → slide up
    AFTER_SLIDE_DOWN,  // fade-out → slide down
    AFTER_PAUSE,       // fade-in  → pause (show background)
    AFTER_SHOW,        // fade-in  → STATE_SHOWN
} AfterFade;

static Window    *s_window;
static Layer     *s_canvas_layer;
static TextLayer *s_time_layer;
static GFont      s_dorblue_font;

// Clock fonts offered in the settings; values match config.js.
typedef enum {
    CLOCK_FONT_DORBLUE = 0,
    CLOCK_FONT_LECO    = 1,
} ClockFont;
static ClockFont  s_clock_font  = CLOCK_FONT_DORBLUE;
static GBitmap   *s_bg_bitmap;
static GBitmap   *s_fg_bitmap;
static AppTimer  *s_anim_timer  = NULL;
static AppTimer  *s_pause_timer = NULL;
static AppTimer  *s_initial_timer = NULL;
#if DEBUG_CYCLE
static AppTimer  *s_debug_timer = NULL;
#endif
static WatchState s_state       = STATE_INIT;
static AfterFade  s_after_fade  = AFTER_NONE;
static int        s_fg_y        = FG_Y_HIDDEN;
static int        s_bg_alpha    = FADE_MAX;  // start with background visible
static int        s_last_fg     = -1;
static bool       s_clock_appeared  = false;
static bool       s_shake_enabled   = true;
static int        s_last_slide_minute = -1;
static char       s_time_buf[6];

static const uint32_t s_fg_res_ids[NUM_FG_IMAGES] = {
    RESOURCE_ID_IMAGE_FRONT_01, RESOURCE_ID_IMAGE_FRONT_02,
    RESOURCE_ID_IMAGE_FRONT_03, RESOURCE_ID_IMAGE_FRONT_04,
    RESOURCE_ID_IMAGE_FRONT_05, RESOURCE_ID_IMAGE_FRONT_06,
    RESOURCE_ID_IMAGE_FRONT_07, RESOURCE_ID_IMAGE_FRONT_08,
    RESOURCE_ID_IMAGE_FRONT_09, RESOURCE_ID_IMAGE_FRONT_10,
    RESOURCE_ID_IMAGE_FRONT_11, RESOURCE_ID_IMAGE_FRONT_12,
    RESOURCE_ID_IMAGE_FRONT_13, RESOURCE_ID_IMAGE_FRONT_14,
    RESOURCE_ID_IMAGE_FRONT_15, RESOURCE_ID_IMAGE_FRONT_16,
};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static void start_fade_out(AfterFade next);
static void start_fade_in(AfterFade next);
static void start_slide_up(void);
static void start_slide_down(void);

// ============================================================================
// HELPERS
// ============================================================================

static void select_and_load_fg(void) {
    int idx;
#if DEBUG_CYCLE
    idx = (s_last_fg + 1) % NUM_FG_IMAGES;
#else
    do {
        idx = rand() % NUM_FG_IMAGES;
    } while (idx == s_last_fg);
#endif
    s_last_fg = idx;
    if (s_fg_bitmap) {
        gbitmap_destroy(s_fg_bitmap);
        s_fg_bitmap = NULL;
    }
    s_fg_bitmap = gbitmap_create_with_resource(s_fg_res_ids[idx]);
}

static void update_time(void) {
#if DEBUG_CYCLE
    if (s_last_fg >= 0) {
        int n = s_last_fg + 1;  // 1-16
        s_time_buf[0] = '0' + n / 10;
        s_time_buf[1] = '0' + n % 10;
        s_time_buf[2] = '\0';
        text_layer_set_text(s_time_layer, s_time_buf);
        return;
    }
#endif
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (!t) return;
    if (clock_is_24h_style()) {
        strftime(s_time_buf, sizeof(s_time_buf), "%H:%M", t);
        text_layer_set_text(s_time_layer, s_time_buf);
    } else {
        strftime(s_time_buf, sizeof(s_time_buf), "%I:%M", t);
        text_layer_set_text(s_time_layer,
            s_time_buf[0] == '0' ? s_time_buf + 1 : s_time_buf);
    }
}

// ============================================================================
// CANVAS
// ============================================================================

static void canvas_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    if (s_bg_bitmap && s_bg_alpha > 0) {
        // Draw full background image
        graphics_context_set_compositing_mode(ctx, GCompOpAssign);
        graphics_draw_bitmap_in_rect(ctx, s_bg_bitmap, bounds);
        // Overlay black scanline stripes to simulate fade
        if (s_bg_alpha < FADE_MAX) {
            int black_rows = FADE_MAX - s_bg_alpha;
            graphics_context_set_fill_color(ctx, GColorBlack);
            for (int y = 0; y < bounds.size.h; y += FADE_MAX) {
                graphics_fill_rect(ctx, GRect(0, y, bounds.size.w, black_rows),
                                   0, GCornerNone);
            }
        }
    } else {
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    }

    // Foreground — alpha composite at current slide position
    if (s_fg_bitmap) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, s_fg_bitmap,
                                     GRect(FG_X, s_fg_y, FG_W, FG_H));
    }
}

// ============================================================================
// ANIMATION TIMER
// ============================================================================

static void pause_timer_callback(void *data);

#if DEBUG_CYCLE
static void debug_timer_callback(void *data) {
    s_debug_timer = NULL;
    if (s_state == STATE_SHOWN) {
        start_fade_out(AFTER_SLIDE_DOWN);
    }
}
#endif

static void anim_timer_callback(void *data) {
    s_anim_timer = NULL;

    switch (s_state) {
        case STATE_FADING_OUT:
            s_bg_alpha--;
            if (s_bg_alpha <= 0) {
                s_bg_alpha = 0;
                layer_mark_dirty(s_canvas_layer);
                AfterFade action = s_after_fade;
                s_after_fade = AFTER_NONE;
                if (action == AFTER_SLIDE_UP) {
                    select_and_load_fg();
                    start_slide_up();
                } else if (action == AFTER_SLIDE_DOWN) {
                    start_slide_down();
                }
                return;
            }
            break;

        case STATE_SLIDING_UP:
            s_fg_y -= SLIDE_STEP;
            if (s_fg_y <= FG_Y_SHOWN) {
                s_fg_y = FG_Y_SHOWN;
                layer_mark_dirty(s_canvas_layer);
                start_fade_in(AFTER_SHOW);
                return;
            }
            break;

        case STATE_SLIDING_DOWN:
            s_fg_y += SLIDE_STEP;
            if (s_fg_y >= FG_Y_HIDDEN) {
                s_fg_y = FG_Y_HIDDEN;
                layer_mark_dirty(s_canvas_layer);
                start_fade_in(AFTER_PAUSE);
                return;
            }
            break;

        case STATE_FADING_IN:
            s_bg_alpha++;
            if (s_bg_alpha >= FADE_MAX) {
                s_bg_alpha = FADE_MAX;
                layer_mark_dirty(s_canvas_layer);
                AfterFade action = s_after_fade;
                s_after_fade = AFTER_NONE;
                if (action == AFTER_SHOW) {
                    s_state = STATE_SHOWN;
                    update_time();
#if DEBUG_CYCLE
                    s_debug_timer = app_timer_register(DEBUG_CYCLE_MS,
                                                       debug_timer_callback, NULL);
#endif
                    if (!s_clock_appeared) {
                        s_clock_appeared = true;
                        layer_set_hidden(text_layer_get_layer(s_time_layer), false);
                    }
                } else if (action == AFTER_PAUSE) {
                    s_state = STATE_PAUSING;
                    s_pause_timer = app_timer_register(PAUSE_DURATION_MS,
                                                       pause_timer_callback, NULL);
                }
                return;
            }
            break;

        default:
            return;
    }

    layer_mark_dirty(s_canvas_layer);
    s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, anim_timer_callback, NULL);
}

static void start_fade_out(AfterFade next) {
    if (s_anim_timer) { app_timer_cancel(s_anim_timer); s_anim_timer = NULL; }
#if DEBUG_CYCLE
    if (s_debug_timer) { app_timer_cancel(s_debug_timer); s_debug_timer = NULL; }
#endif
    s_state = STATE_FADING_OUT;
    s_after_fade = next;
    layer_mark_dirty(s_canvas_layer);
    s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, anim_timer_callback, NULL);
}

static void start_fade_in(AfterFade next) {
    s_state = STATE_FADING_IN;
    s_after_fade = next;
    s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, anim_timer_callback, NULL);
}

static void start_slide_up(void) {
    s_state = STATE_SLIDING_UP;
    s_fg_y = FG_Y_HIDDEN;
    layer_mark_dirty(s_canvas_layer);
    s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, anim_timer_callback, NULL);
}

static void start_slide_down(void) {
    s_state = STATE_SLIDING_DOWN;
    s_fg_y = FG_Y_SHOWN;
    layer_mark_dirty(s_canvas_layer);
    s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, anim_timer_callback, NULL);
}

static void pause_timer_callback(void *data) {
    s_pause_timer = NULL;
    start_fade_out(AFTER_SLIDE_UP);
}

// ============================================================================
// SHAKE / SETTINGS
// ============================================================================

static void tap_handler(AccelAxisType axis, int32_t direction) {
    if (s_state == STATE_SHOWN) {
        start_fade_out(AFTER_SLIDE_DOWN);
    }
}

static void apply_shake_setting(void) {
    if (s_shake_enabled) {
        accel_tap_service_subscribe(tap_handler);
    } else {
        accel_tap_service_unsubscribe();
    }
}

static void apply_clock_font(void) {
    if (!s_time_layer) return;
    Layer *layer = text_layer_get_layer(s_time_layer);
    GRect frame = layer_get_frame(layer);
    if (s_clock_font == CLOCK_FONT_LECO) {
        text_layer_set_font(s_time_layer, fonts_get_system_font(LECO_KEY));
        frame.origin.y = LECO_Y;
        frame.size.h = CLOCK_H - LECO_Y;
    } else {
        text_layer_set_font(s_time_layer, s_dorblue_font);
        frame.origin.y = DORBLUE_Y;
        frame.size.h = CLOCK_H - DORBLUE_Y;
    }
    layer_set_frame(layer, frame);
}

// Clay sends a select's value as a string; accept integers too.
static int32_t tuple_int(const Tuple *t) {
    return t->type == TUPLE_CSTRING ? atoi(t->value->cstring) : t->value->int32;
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    Tuple *font = dict_find(iter, MESSAGE_KEY_CLOCK_FONT);
    if (font) {
        int32_t v = tuple_int(font);
        s_clock_font = (v == CLOCK_FONT_LECO) ? CLOCK_FONT_LECO : CLOCK_FONT_DORBLUE;
        persist_write_int(PERSIST_KEY_FONT, s_clock_font);
        apply_clock_font();
    }

    Tuple *t = dict_find(iter, MESSAGE_KEY_SHAKE_ENABLED);
    if (t) {
        s_shake_enabled = (bool)t->value->int32;
        persist_write_bool(PERSIST_KEY_SHAKE, s_shake_enabled);
        apply_shake_setting();
    }
}

// ============================================================================
// TICK + INITIAL TIMER
// ============================================================================

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();
#if DEBUG_CYCLE
    return;  // the debug timer drives image changes
#endif
    int cur_min = tick_time->tm_min;
    if (s_state == STATE_SHOWN && cur_min != s_last_slide_minute) {
        s_last_slide_minute = cur_min;
        start_fade_out(AFTER_SLIDE_DOWN);
    }
}

static void initial_timer_callback(void *data) {
    s_initial_timer = NULL;
    start_fade_out(AFTER_SLIDE_UP);
}

// ============================================================================
// WINDOW
// ============================================================================

static void window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);

    s_bg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);

    s_canvas_layer = layer_create(bounds);
    layer_set_update_proc(s_canvas_layer, canvas_update_proc);
    layer_add_child(root, s_canvas_layer);

    s_dorblue_font = fonts_load_custom_font(
        resource_get_handle(DORBLUE_RES));
    s_time_layer = text_layer_create(GRect(0, 0, bounds.size.w, CLOCK_H));
    text_layer_set_background_color(s_time_layer, GColorClear);
    text_layer_set_text_color(s_time_layer, GColorWhite);
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
    layer_set_hidden(text_layer_get_layer(s_time_layer), true);
    layer_add_child(root, text_layer_get_layer(s_time_layer));

    apply_clock_font();
    apply_shake_setting();
    uint32_t initial_delay = INITIAL_DELAY_MIN_MS +
        rand() % (INITIAL_DELAY_MAX_MS - INITIAL_DELAY_MIN_MS + 1);
    s_initial_timer = app_timer_register(initial_delay, initial_timer_callback, NULL);
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void window_unload(Window *window) {
    if (s_anim_timer)  { app_timer_cancel(s_anim_timer);  s_anim_timer  = NULL; }
    if (s_pause_timer) { app_timer_cancel(s_pause_timer); s_pause_timer = NULL; }
    if (s_initial_timer) { app_timer_cancel(s_initial_timer); s_initial_timer = NULL; }
#if DEBUG_CYCLE
    if (s_debug_timer) { app_timer_cancel(s_debug_timer); s_debug_timer = NULL; }
#endif
    accel_tap_service_unsubscribe();
    tick_timer_service_unsubscribe();

    if (s_time_layer)  { text_layer_destroy(s_time_layer);  s_time_layer  = NULL; }
    if (s_dorblue_font) { fonts_unload_custom_font(s_dorblue_font); s_dorblue_font = NULL; }
    if (s_canvas_layer){ layer_destroy(s_canvas_layer);     s_canvas_layer = NULL; }
    if (s_fg_bitmap)   { gbitmap_destroy(s_fg_bitmap);      s_fg_bitmap   = NULL; }
    if (s_bg_bitmap)   { gbitmap_destroy(s_bg_bitmap);      s_bg_bitmap   = NULL; }
}

// ============================================================================
// APP LIFECYCLE
// ============================================================================

static void init(void) {
    srand(time(NULL));

    s_clock_font = (persist_read_int(PERSIST_KEY_FONT) == CLOCK_FONT_LECO)
                   ? CLOCK_FONT_LECO : CLOCK_FONT_DORBLUE;
    s_shake_enabled = persist_exists(PERSIST_KEY_SHAKE)
                      ? persist_read_bool(PERSIST_KEY_SHAKE)
                      : true;

    app_message_register_inbox_received(inbox_received_handler);
    app_message_open(64, 64);

    s_window = window_create();
    window_set_background_color(s_window, GColorBlack);
    window_set_window_handlers(s_window, (WindowHandlers){
        .load   = window_load,
        .unload = window_unload,
    });
    window_stack_push(s_window, true);
}

static void deinit(void) {
    if (s_window) {
        window_destroy(s_window);
        s_window = NULL;
    }
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}
