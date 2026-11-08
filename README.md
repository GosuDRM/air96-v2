# Air96 V2 QMK Firmware

QMK firmware for the NuPhy Air96 V2 (ANSI), based on NuPhy's QMK source with
wireless, sleep and USB fixes. Only `keyboards/air96_v2/ansi` is maintained;
the other keyboards were removed.

[![Release](https://img.shields.io/github/v/release/GosuDRM/air96-v2)](https://github.com/GosuDRM/air96-v2/releases/latest)
[![License](https://img.shields.io/badge/license-GPL--2.0--or--later-blue.svg)](LICENSE)

Source version: **v3.2.9** ([CHANGELOG](CHANGELOG.md)). A Rust port lives at
[air96-v2-rs](https://github.com/GosuDRM/air96-v2-rs).

## Flash

Download from [Releases](https://github.com/GosuDRM/air96-v2/releases):

- `air96-v2-c-vX.Y.Z.bin`: default keymap
- `air96-v2-via-vX.Y.Z.bin`: VIA keymap (edit the layout in VIA)

Enter DFU mode: unplug the keyboard, hold **Esc**, plug it in (STM32 DFU,
`0483:df11`). Like QMK bootmagic, this also clears saved settings.

**Linux / macOS**: install `dfu-util` (`apt`, `pacman` or `brew`), then:

```bash
dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D air96-v2-c-vX.Y.Z.bin
```

On Linux, flashing without `sudo` needs the
[QMK udev rules](https://docs.qmk.fm/faq_build#linux-udev-rules).

**Windows**: in [QMK Toolbox](https://github.com/qmk/qmk_toolbox/releases),
select the `.bin`, set the MCU to `STM32F072`, tick **Auto-Flash**, then enter
DFU mode.

## Build

```bash
qmk compile -kb air96_v2/ansi -km default   # or -km via
for t in tests/air96_*_test.py; do python3 "$t"; done
```

Output: `air96_v2_ansi_default.bin` / `air96_v2_ansi_via.bin`. Pushing a `v*`
tag runs the tests, builds both keymaps and publishes a release with that
version's CHANGELOG section.

## Changes from NuPhy's firmware

- NRF UART parsing no longer blocks waiting for ACKs; wireless startup takes
  ~230 ms instead of up to 1250 ms.
- Wireless reports are sent once instead of three times; keys pressed while
  the radio wakes are held until the link connects.
- LEDs and radio power down during USB suspend, even with auto-sleep off.
- Debounce is QMK `asym_eager_defer_pk`, 10 ms: presses register at once,
  releases need 10 ms stable (stock: 1 ms).
- NumLock turns on after power-on, PC restart and wireless reconnect
  (Windows mode).
- Mac Spotlight/screenshot shortcuts take 5 ms instead of 50 ms and keep held
  modifiers.
- Fixes for sleep/wake lockups, mode-switch state leaks, LED flicker and UART
  edge cases.

Full history: [CHANGELOG.md](CHANGELOG.md).

## Wireless limitations

- Each key usage has a fixed wireless report: usages 1–111 use the NKRO
  bitmap, higher usages share six byte slots, so at most six higher usages can
  be held at once. BIOS/boot-only hosts are untested over wireless; use USB
  there.
- The receiver declares interface 1 with `bNumEndpoints=1` but two endpoint
  descriptors (Linux logs the mismatch). NuPhy's 2.1.5 Windows updater image has
  the same defect (SHA-1 `cf6f9a95c0262137942b8842578e195fbae64e6b`, matching the
  [vendor listing](https://nuphy.com.tw/pages/qmk-firmwares)). Keyboard firmware
  can't fix it; it needs a corrected receiver image.
- Host tests check the reports the keyboard emits, not radio delivery or the
  receiver's retry/deduplication.

## License

GPL-2.0-or-later, derived from QMK and NuPhy's keyboard code.
