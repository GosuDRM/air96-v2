# ⌨️ Air96 V2

Optimized QMK firmware for the Air96 V2 wireless mechanical keyboard.

## 🚀 What's Changed

This firmware has been extensively audited and optimized:

- **Bug fixes** — infinite disconnect blink, missing UART checksums, battery data loss, dead code
- **Wireless latency & execution** — startup time 1.25s → 0.23s, reports no longer dropped during pairing, periodic UART congestion reduced 12×, **wait_ack loop latency blockade resolved (rf.c)**
- **Stroke latency** — first-key-after-sleep: lost keystroke + 75ms → 2.7ms preserved, per-report UART time 2.35ms → 0.72ms
- **Power compliance** — forced LED and NRF shutdown during USB suspend in `sleep.c` to fully satisfy USB low-power suspend spec (0.5mA / 2.5mA max)
- **Code smells & static quality** — resolved all brace omissions, static linkages, implicit bool conversions, re-scoped variables dynamically, and removed unreachable/dead structures (ansi.c, rf.c, side.c, sleep.c)
- **Memory footprint optimized** — dynamic loop refactoring and array lookup for battery LEDs reduced binary size by **96 bytes** (down to 56,242 bytes)

See [CHANGELOG.md](../../../CHANGELOG.md) for the full list of changes.

## 🛠️ Build

```bash
# Requires QMK CLI and arm-none-eabi-gcc (Arch/CachyOS: pacman -S qmk arm-none-eabi-gcc)
qmk compile -kb air96_v2/ansi -km default
```

Or with make:

```bash
make air96_v2/ansi:default
```

## ⚡ Flash

Enter bootloader by holding Escape while plugging in, then:

```bash
make air96_v2/ansi:default:flash
```

## 🎛️ VIA Support

```bash
make air96_v2/ansi:via
```

## 🏷️ Version

**v3.0.6** — GosuDRM
STM32F072 · IS31FL3733 RGB driver · 460800 baud UART to NRF wireless module
