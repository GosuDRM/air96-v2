# Air96 V2

Optimized QMK firmware for the Air96 V2 wireless mechanical keyboard.

## What's changed

This firmware has been extensively audited and optimized across 5 passes:

- **Bug fixes** — infinite disconnect blink, missing UART checksums, battery data loss, dead code
- **Wireless latency** — startup time 1.25s → 0.23s, reports no longer dropped during pairing, periodic UART congestion reduced 12×
- **Stroke latency** — first-key-after-sleep: lost keystroke + 75ms → 2.7ms preserved, per-report UART time 2.35ms → 0.72ms
- **Code quality** — deduplicated 4 copy-pasted link-switch handlers, extracted RF init helper, removed dead variables
- **Wireless audit** — sleep/wake race fix, RX buffer bounds check, NRF reset loop hardening, RX timeout recovery, volatile flags, USB-mode power savings

See [CHANGELOG.md](../../../CHANGELOG.md) for the full list of changes.

## Build

```bash
# Requires QMK CLI and arm-none-eabi-gcc (Arch/CachyOS: pacman -S qmk arm-none-eabi-gcc)
qmk compile -kb air96_v2/ansi -km default
```

Or with make:

```bash
make air96_v2/ansi:default
```

## Flash

Enter bootloader by holding Escape while plugging in, then:

```bash
make air96_v2/ansi:default:flash
```

## VIA support

```bash
make air96_v2/ansi:via
```

## Version

**v3.0.2** — GosuDRM
STM32F072 · IS31FL3733 RGB driver · 460800 baud UART to NRF wireless module
