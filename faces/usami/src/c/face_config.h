#pragma once

// Usami's settings for the shared engine (shared/src/c/main.c).

// Foreground images: IMAGE_FRONT_01..NN in package.json.
#define NUM_FG_IMAGES     12
#define FG_RES_IDS { \
    RESOURCE_ID_IMAGE_FRONT_01, RESOURCE_ID_IMAGE_FRONT_02, \
    RESOURCE_ID_IMAGE_FRONT_03, RESOURCE_ID_IMAGE_FRONT_04, \
    RESOURCE_ID_IMAGE_FRONT_05, RESOURCE_ID_IMAGE_FRONT_06, \
    RESOURCE_ID_IMAGE_FRONT_07, RESOURCE_ID_IMAGE_FRONT_08, \
    RESOURCE_ID_IMAGE_FRONT_09, RESOURCE_ID_IMAGE_FRONT_10, \
    RESOURCE_ID_IMAGE_FRONT_11, RESOURCE_ID_IMAGE_FRONT_12, \
}

// Colors: the screen behind the images (also what the background fades to)
// and the clock digits. BG_COLOR must match convert.json's colors.
#define BG_COLOR          PBL_IF_COLOR_ELSE(GColorBrilliantRose, GColorWhite)
#define CLOCK_COLOR       GColorBlack

// Per-platform layout, scaled from emery's. The foreground keeps emery's
// 200:172 shape and sits on the bottom edge; the clock is centered in the
// band above it. Keep FG sizes in sync with shared/tools/convert_images.py.
// CUSTOM_FONT_* is the bundled clock font (DorBlue here); LECO_* is
// Pebble's built-in alternative offered in the settings.
#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE) || \
    defined(PBL_PLATFORM_DIORITE)       // 144x168 (aplite/diorite: B&W)
#define FG_X              0
#define FG_W              144
#define FG_H              124
#define CUSTOM_FONT_RES   RESOURCE_ID_FONT_DORBLUE_34
#define CUSTOM_FONT_Y     -1
#define LECO_KEY          FONT_KEY_LECO_32_BOLD_NUMBERS
#define LECO_Y            2
#define SLIDE_STEP        9
#elif defined(PBL_PLATFORM_CHALK)       // 180x180 round
#define FG_X              19
#define FG_W              142
#define FG_H              123
#define CUSTOM_FONT_RES   RESOURCE_ID_FONT_DORBLUE_32
#define CUSTOM_FONT_Y     15
#define LECO_KEY          FONT_KEY_LECO_32_BOLD_NUMBERS
#define LECO_Y            16
#define SLIDE_STEP        9
#elif defined(PBL_PLATFORM_GABBRO)      // 260x260 round
#define FG_X              24
#define FG_W              212
#define FG_H              182
#define CUSTOM_FONT_RES   RESOURCE_ID_FONT_DORBLUE_46
#define CUSTOM_FONT_Y     20
#define LECO_KEY          FONT_KEY_LECO_42_NUMBERS
#define LECO_Y            24
#define SLIDE_STEP        13
#else                                   // emery, 200x228
#define FG_X              0
#define FG_W              200
#define FG_H              172
#define CUSTOM_FONT_RES   RESOURCE_ID_FONT_DORBLUE_48
#define CUSTOM_FONT_Y     -5    // nudges DorBlue to center it in the clock band
#define LECO_KEY          FONT_KEY_LECO_42_NUMBERS
#define LECO_Y            0
#define SLIDE_STEP        12    // pixels per frame
#endif
