# Monokuma Appears

A Pebble watchface for **emery** (Pebble Time 2). Monokuma appears over a background in one of 16 poses. On launch, the opening image stays on screen for a random 1–3 seconds before it fades and the clock appears. Shaking or tapping the watch switches to a different image.

## Settings

Settings use [Clay](https://github.com/pebble/clay) and open from the watchface's gear icon in the Pebble phone app:

- **Change image on shake** (`SHAKE_ENABLED`, on by default): whether shaking or tapping the watch switches the image.

To try the settings page in the emulator, run `pebble emu-app-config --emulator emery`.

## Build and run

```sh
pebble build
pebble install --emulator emery
```

## Layout

- `src/c/main.c`: the watchface
- `src/pkjs/index.js`: phone-side JS (starts Clay)
- `src/pkjs/config.js`: Clay settings page definition
- `resources/images/`: background and the 16 `front01`–`front16` images
