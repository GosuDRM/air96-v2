<p align="center">
  <a href="CHANGELOG.md"><img src="https://img.shields.io/badge/version-v3.2.8-blue.svg?style=for-the-badge" alt="Version"/></a>
  <img src="https://img.shields.io/badge/mcu-STM32F072-orange.svg?style=for-the-badge" alt="MCU"/>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--2.0-green.svg?style=for-the-badge" alt="License"/></a>
  <img src="https://img.shields.io/badge/status-stable-brightgreen.svg?style=for-the-badge" alt="Status"/>
</p>

# ⌨️ Air96 V2 — NuPhy QMK Firmware

Current source version: **v3.2.8**. See [release notes](CHANGELOG.md#v328-2026-11-07).

Welcome to the high-performance QMK firmware optimization fork for the **NuPhy Air96 V2** wireless mechanical keyboard. This repository is dedicated to pushing the performance, latency, and power-efficiency boundaries of the Air96 V2 hardware.

Only the `air96_v2/ansi` keyboard is maintained. All other keyboards have been removed to keep this repository focused, lightweight, and streamlined.

## 🛠️ Build

```bash
qmk compile -kb air96_v2/ansi -km default
```

Binary output: `air96_v2_ansi_default.bin`

## ⚡ Flash

📥 **[Release Firmware (air96-v2-c-v3.2.8.bin)](https://github.com/GosuDRM/air96-v2/releases/download/v3.2.8/air96-v2-c-v3.2.8.bin)**

Hold the **Escape** key while plugging in the USB cable to enter DFU mode, then follow the instructions for your operating system:

### 🐧 Linux
Install `dfu-util` via your package manager (e.g. `sudo apt install dfu-util` or `sudo pacman -S dfu-util`), then run:
```bash
dfu-util -d 0483:DF11 -a 0 -s 0x08000000:leave -D air96-v2-c-v3.2.8.bin
```
*Note: You may need `sudo` or to configure [QMK udev rules](https://docs.qmk.fm/faq_build#linux-udev-rules) to flash without root privileges.*

### 🍎 macOS
Install `dfu-util` via Homebrew:
```bash
brew install dfu-util
dfu-util -d 0483:DF11 -a 0 -s 0x08000000:leave -D air96-v2-c-v3.2.8.bin
```
Alternatively, you can download and use the graphical [QMK Toolbox](https://github.com/qmk/qmk_toolbox/releases).

### 🪟 Windows
1. Download and run the graphical [QMK Toolbox](https://github.com/qmk/qmk_toolbox/releases).
2. Select `air96_v2_ansi_default.bin` as the Local File.
3. Set the Microcontroller to `STM32F072`.
4. Check the **Auto-Flash** checkbox.
5. Hold the **Escape** key and plug in the USB cable. The flashing process will start automatically.

## 🌟 Highlights (v3.0.0 → v3.2.1)

This firmware has undergone extensive audit, bug-fixing, and performance hardening compared to the original base port. Below are the notable achievements across the **v3.x** branch:

* **🔄 Stability Overhaul (v3.2.0):**
  Multi-pass audit fixing sleep/wake lockup, mode-switching state leaks, LED flicker, and UART protocol edge cases. NRF sleep requests are now rejected when USB is active, all sync counters reset on mode switch, battery indicator auto-dismisses after 10s, and key debounce was raised from 1ms to 5ms. Current source uses 10ms asymmetric per-key debounce: immediate presses, 10ms stable releases.
* **⚡ Wake & Command Hardening:**
  Solved a critical wait-for-ACK loop blocking issue in `rf.c`, restoring immediate, non-blocking serial parsing. Wireless startup dead-time has been slashed **5.4×** (1250ms → 230ms), and current source buffers wake-key transitions until the selected radio channel reports connected. Physical wake latency and delivery require hardware validation.
* **🔋 Power Consumption & USB Compliance:**
  Prescaled state synchronization checks in wired mode to save 2-3mA on battery. The sleep handler has been hardened to force LED and NRF power-downs during USB suspend even when auto-sleep is disabled, fully satisfying the USB suspend current draw limits (0.5mA / 2.5mA max).
* **🧠 Layout & UX Refinements:**
  Added deferred NumLock auto-on signaling synchronized with USB enumeration to match high-end custom firmware. Reduced Spotlight and screenshot key chord blocks from 50ms to 5ms for rapid, lag-free OS registration.
* **🧹 Code Quality & Footprint Optimization:**
  Eliminated all brace omissions, implicit boolean conversions, and scoped local variables to their narrowest blocks. Transitioned the battery LED indicator to a dynamic loop with lookup arrays, optimizing compiler generation and **limiting the final binary footprint to 56,862 bytes** (including all custom features).
* **📜 Full Revision History:**
  See [CHANGELOG.md](CHANGELOG.md) for the complete record of all engineering optimizations, bug fixes, and releases.

### 📊 Performance & Optimization Summary

| Metric / Feature | Upstream (v3.0.0 Base) | Optimized Latest (v3.2.1) | Impact & Improvement |
| :--- | :--- | :--- | :--- |
| **Startup Dead-Time** | `1250ms` (worst-case) | `~230ms` | **5.4× faster boot-up** & instant availability |
| **Active Periodic UART Utilization** | `11.3%` | `0.9%` | **12.5× reduced wireless bus congestion** |
| **Per-Report UART Payload Time** | `2.35ms` (brute 3× repeats) | `0.72ms` (reliable 1× transmit) | **3.26× faster keystroke packet dispatch** |
| **Keystroke Transmission Latency** | `~4.0ms` | `~1ms` | **Ultra-low latency** key-press registration |
| **Deep-Sleep First-Key Wakeup (current source)** | Keystroke lost + `75ms` delay | Buffered until selected-channel connection | Simulated ordered delivery; hardware validation pending |
| **Mac Spotlight/Screenshot Lag** | `50ms` key-chord delay | `5ms` optimized chord delay | **Eliminated sluggish OS-shortcut latency** |
| **USB Suspend Power Draw** | Exceeded limits (up to `20mA`) | Compliant **< 2.5mA** limit | **100% Standard USB Compliance** (stops battery drain) |
| **UART Loop Transmit** | Blocked 5–10ms per wireless cmd | **Non-blocking (0ms)** concurrent polling | **Zero boot/wakeup locks**, flawless channel switching |
| **Key Debounce (current source)** | `1ms` | `10ms` asym_eager_defer_pk | Filters tested contact chatter; no press delay, releases need 10ms stable |
| **Binary Footprint** | `56,490` bytes | `56,862` bytes | **Feature-rich layout** optimized for performance and completeness |

## ✨ Features

- Bluetooth LE + 2.4 GHz wireless (NRF52832 module)
- Wired USB HID (keyboard + consumer + system control)
- 5-layer keymap (Mac/Win base + Fn layers)
- Side LED animations (wave/mix/static/breath/off)
- Dual IS31FL3733 RGB matrix (110 LEDs)
- EEPROM config persistence
- DFU bootloader entry (Escape key at boot)

## 📜 License

GPL-2.0-or-later — derived from QMK firmware and NuPhy's keyboard code.


## Wireless receiver limitations

Current source gives each key usage a fixed report interface to prevent held-key
migration during rollover: usages 1..111 use the bitmap report; higher usages
share six byte-report slots. More than six simultaneous higher usages cannot be
represented by this protocol. Ordinary wireless typing now relies on the NKRO
interface; BIOS/boot-only host compatibility is unverified. Use wired USB there.

The connected receiver and the application image in NuPhy's official 2.1.5
Windows updater both contain interface 1 with `bNumEndpoints=1` followed by two
endpoint descriptors. Linux logs the mismatch. The updater's SHA-1 matches the
[published vendor listing](https://nuphy.com.tw/pages/qmk-firmwares):
`cf6f9a95c0262137942b8842578e195fbae64e6b`.
Installing that package is not a demonstrated fix for this descriptor defect.
Receiver source or a corrected vendor image is needed to repair it.

Host regression tests validate emitted reports and modeled interface transitions,
not radio delivery. Hardware acceptance still needs idle/wake taps, rapid taps,
held-key rollover, disconnect/reconnect releases, and both receiver interfaces
observed together. Retry/deduplication and buffer behavior inside the receiver
remain unverified. The source changes do not update receiver firmware.
