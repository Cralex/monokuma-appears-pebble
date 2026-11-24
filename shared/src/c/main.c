#include <pebble.h>

// ============================================================================
// CONSTANTS
// ============================================================================

// Face-specific settings: image count and resource IDs, per-platform layout
// and clock font. Each face provides its own in faces/<name>/src/c/.
#include "face_config.h"
#include "minute_counter.h"

#define SCREEN_H          PBL_DISPLAY_HEIGHT
// s_fg_y is the top of the FG_W x FG_H foreground area. An image can be
// taller than FG_H; the extra rows reach up into the clock band above it.
#define FG_Y_SHOWN        (SCREEN_H - FG_H)
#define FG_Y_HIDDEN       SCREEN_H   // for an image FG_H tall; see fg_y_hidden()
#define CLOCK_H           FG_Y_SHOWN
#define ANIM_INTERVAL_MS  33    // ~30 fps
#define FADE_MAX          4     // fade steps; 4 * 33ms = 132ms per fade
#define PAUSE_DURATION_MS 1000  // background-only pause between fg changes
#define INITIAL_DELAY_MIN_MS 1000  // random boot delay before first fade/clock
#define INITIAL_DELAY_MAX_MS 3000
#define PERSIST_KEY_SHAKE 0
#define PERSIST_KEY_FONT  1
// Keys 2-6: the optional minute counter (minute_counter.c).

// Debug: show every image in order, one every DEBUG_CYCLE_MS once it's fully
// shown, with the image number (01-NN) in place of the time. Set to 0 for
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
static Layer     *s_time_layer;   // the clock, drawn by time_update_proc
static GFont      s_custom_font;
static GFont      s_clock_gfont;   // the font in use (custom or LECO)
static const char *s_time_text = "";

// Clock fonts offered in the settings; values match config.js.
typedef enum {
    CLOCK_FONT_CUSTOM = 0,
    CLOCK_FONT_LECO    = 1,
} ClockFont;
static ClockFont  s_clock_font  = CLOCK_FONT_CUSTOM;
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

static const uint32_t s_fg_res_ids[NUM_FG_IMAGES] = FG_RES_IDS;

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

static void set_time_text(const char *text) {
    s_time_text = text;
    if (s_time_layer) layer_mark_dirty(s_time_layer);
}

// Rows the current image reaches above the foreground area.
static int fg_extra_h(void) {
    return s_fg_bitmap ? gbitmap_get_bounds(s_fg_bitmap).size.h - FG_H : 0;
}

// s_fg_y at which the current image is fully below the screen.
static int fg_y_hidden(void) {
    return FG_Y_HIDDEN + fg_extra_h();
}

static void update_time(void) {
#if DEBUG_CYCLE
    if (s_last_fg >= 0) {
        int n = s_last_fg + 1;  // 1-NUM_FG_IMAGES
        s_time_buf[0] = '0' + n / 10;
        s_time_buf[1] = '0' + n % 10;
        s_time_buf[2] = '\0';
        set_time_text(s_time_buf);
        return;
    }
#endif
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (!t) return;
    if (clock_is_24h_style()) {
        strftime(s_time_buf, sizeof(s_time_buf), "%H:%M", t);
        set_time_text(s_time_buf);
    } else {
        strftime(s_time_buf, sizeof(s_time_buf), "%I:%M", t);
        set_time_text(s_time_buf[0] == '0' ? s_time_buf + 1 : s_time_buf);
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
        // Overlay BG_COLOR scanline stripes to simulate fade
        if (s_bg_alpha < FADE_MAX) {
            int stripe_rows = FADE_MAX - s_bg_alpha;
            graphics_context_set_fill_color(ctx, BG_COLOR);
            for (int y = 0; y < bounds.size.h; y += FADE_MAX) {
                graphics_fill_rect(ctx, GRect(0, y, bounds.size.w, stripe_rows),
                                   0, GCornerNone);
            }
        }
    } else {
        graphics_context_set_fill_color(ctx, BG_COLOR);
        graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    }

    // Foreground — alpha composite at current slide position
    if (s_fg_bitmap) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, s_fg_bitmap,
                                     GRect(FG_X, s_fg_y - fg_extra_h(),
                                           FG_W, FG_H + fg_extra_h()));
    }
}

// ============================================================================
// CLOCK
// ============================================================================

static void draw_time(GContext *ctx, GRect box, GColor color) {
    graphics_context_set_text_color(ctx, color);
    graphics_draw_text(ctx, s_time_text, s_clock_gfont, box,
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

#ifdef CLOCK_OUTLINE_COLOR
// The outline shows only where the digits cross the image (a pose reaching
// into the clock band), not over the plain background. On color screens the
// update proc notes which pixels under the clock are plain BG_COLOR before
// drawing, then turns the outline back to BG_COLOR there. On black-and-white
// screens the background is white like the outline, so no fix-up is needed.
#if defined(PBL_COLOR)
#define CLOCK_MASK_BYTES  ((PBL_DISPLAY_WIDTH * CLOCK_H + 7) / 8)
static uint8_t s_bg_mask[CLOCK_MASK_BYTES];  // 1 = plain background pixel

// For each screen pixel in rows [0, rows): record (record=true) whether it
// is BG_COLOR, or restore (record=false) outline pixels recorded as BG_COLOR.
static void scan_clock_band(GContext *ctx, int rows, bool record) {
    GBitmap *fb = graphics_capture_frame_buffer(ctx);
    if (!fb) return;
    const uint8_t bg = BG_COLOR.argb;
    const uint8_t outline = CLOCK_OUTLINE_COLOR.argb;
    for (int y = 0; y < rows; y++) {
        GBitmapDataRowInfo row = gbitmap_get_data_row_info(fb, y);
        for (int x = row.min_x; x <= row.max_x; x++) {
            int i = y * PBL_DISPLAY_WIDTH + x;
            uint8_t bit = 1 << (i & 7);
            if (record) {
                if (row.data[x] == bg) s_bg_mask[i >> 3] |= bit;
                else                   s_bg_mask[i >> 3] &= ~bit;
            } else if ((s_bg_mask[i >> 3] & bit) && row.data[x] == outline) {
                row.data[x] = bg;
            }
        }
    }
    graphics_release_frame_buffer(ctx, fb);
}
#endif
#endif

static void time_update_proc(Layer *layer, GContext *ctx) {
    GRect box = layer_get_bounds(layer);
#ifdef CLOCK_OUTLINE_COLOR
    // The layer's rows on screen that the clock can draw in.
    GRect frame = layer_get_frame(layer);
    int rows = frame.origin.y + frame.size.h;
    if (rows > CLOCK_H) rows = CLOCK_H;
#if defined(PBL_COLOR)
    scan_clock_band(ctx, rows, true);
#endif
    const int w = CLOCK_OUTLINE_W;
    for (int dy = -w; dy <= w; dy++) {
        for (int dx = -w; dx <= w; dx++) {
            if ((dx || dy) && dx * dx + dy * dy <= w * w + w) {
                draw_time(ctx, GRect(box.origin.x + dx, box.origin.y + dy,
                                     box.size.w, box.size.h),
                          CLOCK_OUTLINE_COLOR);
            }
        }
    }
    draw_time(ctx, box, CLOCK_COLOR);
#if defined(PBL_COLOR)
    scan_clock_band(ctx, rows, false);
#endif
    (void)rows;
#else
    draw_time(ctx, box, CLOCK_COLOR);
#endif
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
            if (s_fg_y >= fg_y_hidden()) {
                s_fg_y = fg_y_hidden();
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
                        layer_set_hidden(s_time_layer, false);
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
    s_fg_y = fg_y_hidden();
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
    Layer *layer = s_time_layer;
    GRect frame = layer_get_frame(layer);
    if (s_clock_font == CLOCK_FONT_LECO) {
        s_clock_gfont = fonts_get_system_font(LECO_KEY);
        frame.origin.y = LECO_Y;
        frame.size.h = CLOCK_H - LECO_Y;
    } else {
        s_clock_gfont = s_custom_font;
        frame.origin.y = CUSTOM_FONT_Y;
        frame.size.h = CLOCK_H - CUSTOM_FONT_Y;
    }
    layer_set_frame(layer, frame);
}

// Clay sends a select's value as a string; accept integers too.
static int32_t tuple_int(const Tuple *t) {
    return t->type == TUPLE_CSTRING ? atoi(t->value->cstring) : t->value->int32;
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    minute_counter_inbox(iter);

    Tuple *font = dict_find(iter, MESSAGE_KEY_CLOCK_FONT);
    if (font) {
        int32_t v = tuple_int(font);
        s_clock_font = (v == CLOCK_FONT_LECO) ? CLOCK_FONT_LECO : CLOCK_FONT_CUSTOM;
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
    minute_counter_tick();
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

    s_custom_font = fonts_load_custom_font(
        resource_get_handle(CUSTOM_FONT_RES));
    s_time_layer = layer_create(GRect(0, 0, bounds.size.w, CLOCK_H));
    layer_set_update_proc(s_time_layer, time_update_proc);
    layer_set_hidden(s_time_layer, true);
    layer_add_child(root, s_time_layer);

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

    if (s_time_layer)  { layer_destroy(s_time_layer);       s_time_layer  = NULL; }
    if (s_custom_font) { fonts_unload_custom_font(s_custom_font); s_custom_font = NULL; }
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
                   ? CLOCK_FONT_LECO : CLOCK_FONT_CUSTOM;
    s_shake_enabled = persist_exists(PERSIST_KEY_SHAKE)
                      ? persist_read_bool(PERSIST_KEY_SHAKE)
                      : true;
    minute_counter_load();

    app_message_register_inbox_received(inbox_received_handler);
    app_message_open(64, 400);  // outbox: the minute counter's spans

    s_window = window_create();
    window_set_background_color(s_window, BG_COLOR);
    window_set_window_handlers(s_window, (WindowHandlers){
        .load   = window_load,
        .unload = window_unload,
    });
    window_stack_push(s_window, true);
}

static void deinit(void) {
    minute_counter_save();
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
