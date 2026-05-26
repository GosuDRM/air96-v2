# ⌨️ Air96 V2 — NuPhy QMK Firmware

Fork of QMK firmware for the NuPhy Air96 V2 wireless mechanical keyboard.

Only the `air96_v2/ansi` keyboard is maintained. All other keyboards have been
removed to keep this repository focused and lightweight.

## 🛠️ Build

```bash
qmk compile -kb air96_v2/ansi -km default
```

Binary output: `air96_v2_ansi_default.bin`

## ⚡ Flash

Hold the **Escape** key while plugging in the USB cable to enter DFU mode, then follow the instructions for your operating system:

### 🐧 Linux
Install `dfu-util` via your package manager (e.g. `sudo apt install dfu-util` or `sudo pacman -S dfu-util`), then run:
```bash
dfu-util -d 0483:DF11 -a 0 -s 0x08000000:leave -D air96_v2_ansi_default.bin
```
*Note: You may need `sudo` or to configure [QMK udev rules](https://docs.qmk.fm/faq_build#linux-udev-rules) to flash without root privileges.*

### 🍎 macOS
Install `dfu-util` via Homebrew:
```bash
brew install dfu-util
dfu-util -d 0483:DF11 -a 0 -s 0x08000000:leave -D air96_v2_ansi_default.bin
```
Alternatively, you can download and use the graphical [QMK Toolbox](https://github.com/qmk/qmk_toolbox/releases).

### 🪟 Windows
1. Download and run the graphical [QMK Toolbox](https://github.com/qmk/qmk_toolbox/releases).
2. Select `air96_v2_ansi_default.bin` as the Local File.
3. Set the Microcontroller to `STM32F072`.
4. Check the **Auto-Flash** checkbox.
5. Hold the **Escape** key and plug in the USB cable. The flashing process will start automatically.

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
