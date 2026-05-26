# Air96 V2 — NuPhy QMK Firmware

Fork of QMK firmware for the NuPhy Air96 V2 wireless mechanical keyboard.

Only the `air96_v2/ansi` keyboard is maintained. All other keyboards have been
removed to keep this repository focused and lightweight.

## Build

```bash
qmk compile -kb air96_v2/ansi -km default
```

Binary output: `air96_v2_ansi_default.bin`

## Flash

Hold Escape while plugging in USB to enter DFU mode, then:

```bash
dfu-util -d 0483:DF11 -a 0 -s 0x08000000:leave -D air96_v2_ansi_default.bin
```

## Features

- Bluetooth LE + 2.4 GHz wireless (NRF52832 module)
- Wired USB HID (keyboard + consumer + system control)
- 5-layer keymap (Mac/Win base + Fn layers)
- Side LED animations (wave/mix/static/breath/off)
- Dual IS31FL3733 RGB matrix (110 LEDs)
- EEPROM config persistence
- DFU bootloader entry (Escape key at boot)

## License

GPL-2.0-or-later — derived from QMK firmware and NuPhy's keyboard code.
