<p align="center">
  <img src="https://img.shields.io/badge/keyboard-NuPhy%20Air96%20V2-brightgreen.svg?style=for-the-badge" alt="Keyboard"/>
  <img src="https://img.shields.io/badge/mcu-STM32F072-orange.svg?style=for-the-badge" alt="MCU"/>
  <img src="https://img.shields.io/badge/status-stable-brightgreen.svg?style=for-the-badge" alt="Status"/>
</p>

# ⌨️ Air96 V2 Keyboard Port

Optimized high-performance QMK firmware port for the Air96 V2 wireless mechanical keyboard.

## 🚀 What's Changed

This firmware has been extensively audited, bug-fixed, and performance-hardened compared to the stock version:

- **⚡ Zero-Latency Wake & Command Hardening** — Solved a critical wait-for-ACK loop blocking issue in `rf.c`, restoring immediate, non-blocking serial parsing. Wireless startup dead-time has been slashed **5.4×** (1250ms → 230ms), and inline wakeup logic ensures that the first keystroke after deep sleep is preserved and registered within `2.7ms` (previously lost + 75ms).
- **🔋 Power Consumption & USB Compliance** — Prescaled state synchronization checks in wired mode to save 2-3mA on battery. The sleep handler has been hardened to force LED and NRF power-downs during USB suspend even when auto-sleep is disabled, fully satisfying the USB suspend current draw limits (0.5mA / 2.5mA max).
- **🧠 Layout & UX Refinements** — Added deferred NumLock auto-on signaling synchronized with USB enumeration to match high-end custom firmware. Reduced Spotlight and screenshot key chord blocks from 50ms to 5ms for rapid, lag-free OS registration.
- **🧹 Code Quality & Footprint Optimization** — Eliminated all brace omissions, implicit boolean conversions, and scoped local variables to their narrowest blocks. Transitioned the battery LED indicator to a dynamic loop with lookup arrays, optimizing compiler generation and **shrinking the final binary footprint to 56,366 bytes** (down from 56,490 bytes).
- **📜 Full Revision History** — See [CHANGELOG.md](../../../CHANGELOG.md) for the complete record of all optimizations, bug fixes, and releases.

## 🛠️ Build

Requires QMK CLI and `arm-none-eabi-gcc` toolchain.

```bash
qmk compile -kb air96_v2/ansi -km default
```

Or build with standard GNU Make:

```bash
make air96_v2/ansi:default
```

## ⚡ Flash

📥 **[Download Pre-compiled Release Firmware (air96-v2-c-v3.0.7.bin)](https://github.com/GosuDRM/air96-v2/releases/download/v3.0.7/air96-v2-c-v3.0.7.bin)**

1. Disconnect the keyboard's USB cable.
2. Hold down the **Escape** key while plugging in the USB cable to enter DFU bootloader mode.
3. Execute the flash command:
```bash
make air96_v2/ansi:default:flash
```

## 🎛️ VIA Support

To compile the firmware with VIA layout editing support, use the `via` keymap:

```bash
make air96_v2/ansi:via
```

## 🏷️ Version Specs

- **Version:** `v3.0.7`
- **Controller:** STM32F072
- **RGB Matrix Driver:** Dual IS31FL3733 (110 individual RGB LEDs)
- **Wireless Interface:** 460800 baud hardware UART interface to NRF52832 module

