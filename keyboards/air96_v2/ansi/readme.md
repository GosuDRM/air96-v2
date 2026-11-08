# Air96 V2 (ANSI)

NuPhy Air96 V2, a 96% wireless keyboard (USB, Bluetooth LE, 2.4 GHz).

* Hardware: STM32F072, NRF52832 radio module (UART, 460800 baud),
  2× IS31FL3733 (110 RGB LEDs)
* Keymaps: `default`, `via`

Make example for this keyboard (after setting up your build environment):

    qmk compile -kb air96_v2/ansi -km default

Flashing example for this keyboard:

    make air96_v2/ansi:default:flash

See the [build environment setup](https://docs.qmk.fm/newbs_getting_started)
and the [make instructions](https://docs.qmk.fm/getting_started_make_guide).

## Bootloader

Hold **Esc** while plugging in the USB cable (bootmagic). This also clears
saved settings.

Changes from NuPhy's firmware are listed in the repository
[README](../../../README.md) and [CHANGELOG](../../../CHANGELOG.md).
