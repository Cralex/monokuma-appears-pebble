# Monokuma Appears

A Pebble watchface for **emery** (Pebble Time 2). Monokuma appears over a background in one of 16 poses. Shaking or tapping the watch switches to a different image; you can turn this off in the phone settings (`SHAKE_ENABLED`).

## Build and run

```sh
pebble build
pebble install --emulator emery
```

## Layout

- `src/c/main.c`: the watchface
- `src/pkjs/index.js`: phone-side JS (settings)
- `resources/images/`: background and the 16 `front01`–`front16` images
