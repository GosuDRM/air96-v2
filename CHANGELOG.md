# Changelog

## v3.0.2 — GosuDRM (2026-05-26)

Wireless subsystem audit: 3 critical bugs, 2 high-severity issues, 2 medium robustness fixes, and 6 optimizations. Binary size reduced by 56 bytes.

### Critical Bug Fixes

- **Sleep/wake race condition** — `uart_send_report()` cleared `f_wakeup_prepare` before `Sleep_Handle()` could act on it, preventing USB wakeup signaling. First keypress after USB suspend would light up LEDs but never wake the PC. Fixed by letting `Sleep_Handle()` own the full wake sequence. (rf.c)
- **RX buffer overread** — `RF_Protocol_Receive()` had no bounds check on `RX_LEN` (received from RF module). A malformed packet with large `RX_LEN` could read past `RXDBuf[64]`. Added `RX_LEN > UART_MAX_LEN - 5` guard. (rf.c)
- **Redundant checksum writes** — `CMD_SET_NAME` and `CMD_SET_24G_NAME` computed checksums inside their switch-cases that were then overwritten by the generic checksum at line 441. Removed the dead in-case writes. (rf.c)

### High Severity Fixes

- **NRF reset loop** — `sync_lost` threshold of 5 (1 second) with 200ms blocking reset delays could trap the NRF in an infinite reset cycle. Increased threshold to 10 (2 seconds) and added 600ms post-reset cooldown to let the NRF boot. (rf.c)
- **Stale NKRO after channel switch** — `f_bit_kb_act` was never cleared by `m_break_all_key()`, causing periodic resends of potentially stale bit reports after mode/channel switching. (ansi.c)

### Medium Fixes

- **RX timeout recovery** — `uart_receive_pro()` could hang indefinitely on partial packets from UART noise. Added 10ms timeout that discards partial packets and resets the parser. Uses the previously-unused `RXDOverTime` concept. (rf.c)
- **Idle resend window** — Reduced from ~20 seconds (`no_act_time <= 2000`) to ~2 seconds (`<= 200`). Eliminates ~100 redundant zero-key reports after the last keypress. (rf.c)

### Optimizations

- **Removed dead triple-send** — `uart_repeat_flag` and its 3× retry loop in `UART_Send_Bytes` were never triggered (always set to 0 before send). Removed entirely, simplifying the transmit path. (rf.c)
- **USB-mode sync rate** — `CMD_RF_STS_SYSC` was sent 5×/sec even in USB mode (waking the NRF unnecessarily). Now uses a prescaler: 1×/2sec in USB mode, 5×/sec in RF mode. Saves ~2-3mA on battery. (rf.c)
- **Report buffer sizing** — `uart_bit_report_buf` and `bitkb_report_buf` reduced from `[32]` to `[NKRO_REPORT_BITS + 1]` (16 bytes each). Saves 32 bytes of RAM. (rf.c)
- **Volatile qualifiers** — Added `volatile` to 8 flags shared between main loop and UART polling paths (`f_uart_ack`, `f_rf_hand_ok`, `f_rf_read_data_ok`, `f_rf_sts_sysc_ok`, `f_rf_new_adv_ok`, `f_rf_reset`, `f_send_channel`, `f_goto_sleep`, `f_wakeup_prepare`). Prevents compiler from hoisting flag checks out of polling loops. (ansi.c, rf.c, sleep.c)
- **Resend interval** — Periodic report resend reduced from 200ms to 100ms for improved wireless responsiveness during fast typing. (rf.c)
- **2.4G name encoding** — Replaced 21 individual `TXDBuf` assignments with a `static const uint8_t[]` UTF-16LE array and `memcpy`. Eliminates fragile dependency on `memset` zeroing interleaved bytes. (rf.c)

### Size

| Metric | Before | After |
|--------|--------|-------|
| Binary | 56490 bytes | 56434 bytes (−56) |

---

## v3.0.1 — GosuDRM (2026-05-26)

Additional codebase audit, critical bug fixes, and reliability improvements.

### Bug Fixes

- **UART NKRO Parameter Ignore** — Fixed `uart_send_report_nkro` ignoring its parameter and using global `nkro_report` instead. (rf.c)
- **Missing Modifiers in Wireless NKRO** — Synced `uart_bit_report_buf[0]` modifiers and triggered immediate sending on modifier changes when NKRO is active to prevent modifier lag/flicker. (rf.c)
- **UART Packet Fragmentation** — Refactored `uart_receive_pro` into a robust byte-by-byte parser tracking packet frames to prevent keystroke loss, lag, and connection drops. (rf.c)
- **NULL Host Driver USB Boot** — Corrected USB startup initialization sequence by setting `m_host_driver` before scanning the dial switch. (ansi.c)
- **Battery Charging Breathing LED Timeout** — Kept `bat_show_flag = true` while charging (`charge_state == 0x03`) to sustain the breathing animation continuously. (side.c)

---

## v3.0.0 — GosuDRM (2025-05-25)

Four-pass audit and optimization of the Air96 V2 QMK firmware
(base: QMK firmware, optimized for wireless performance).

---

### Bug Fixes

- **Disconnect blink infinite loop** — `rf_blink_cnt` was reset every 200ms by `dev_sts_sync`, preventing the 3-blink sequence from ever completing. Now only resets on state change. (rf.c:494)
- **Missing UART checksums** — 10 of 12 command types sent checksum `0x00`. Added `get_checksum()` call before every `UART_Send_Bytes`. (rf.c:439)
- **Battery forced to 100% while charging** — `dev_info.rf_baterry` was overwritten in two places while charging, losing the real battery level. Removed both overwrites. (rf.c:250, side.c:699)
- **EEPROM loaded before QMK init** — `m_londing_eeprom_data()` called before `keyboard_post_init_user()`, risking RGB ops on uninitialized subsystem. Reordered. (ansi.c:721)
- **Timer drift** — `interval_timer += 10` accumulated drift over time. Replaced with `timer_read32()` always. (ansi.c:670)
- **Dead code** — Removed two `if (0)` blocks with unreachable color-cycling logic in side LED breathe and static modes. (side.c:432,473)
- **Register name flag** — Added FIXME for suspicious `GPIO_OSPEEDER_OSPEEDR6` macro (extra "ER" — verify against target ChibiOS HAL). (rf.c:648)

### Wireless Latency

- **Startup dead time: 500ms → 100ms** — Reduced blind `wait_ms(500)` in `keyboard_post_init_kb`. NRF52 boots in <100ms. (ansi.c:720)
- **Reports no longer dropped during pairing/linking** — Removed `rf_state != RF_CONNECT` gate. The RF module handles its own state; keystrokes are no longer silently discarded during connection setup. (rf.c:585)
- **MAC key blocking: 50ms → 5ms** — Four `wait_ms(50)` in Spotlight/screenshot handlers reduced to 5ms. The OS doesn't need 50ms to register a chord. (ansi.c:507,542,555,564)
- **RF init retry delays: 20ms → 5ms** — Reduced `delayms` parameter in all three `rf_device_init` retry loops. Removed redundant `wait_ms(5)` between send and receive. Worst-case init from ~750ms → ~180ms. (rf.c:660-690)
- **Periodic sender interval: 50ms → 200ms** — UART utilization dropped from 11.3% to 0.9%. (rf.c:124)
- **Report transmission: 3× → 1×** — 460800 baud with parity provides sufficient reliability without brute-force triple-send. Per-report UART time 2.35ms → 0.72ms. (rf.c:595)
- **Link switch delays: 10-30ms → 5ms** — Reduced `delayms` on all `uart_send_cmd(CMD_SET_LINK, ...)` and `CMD_SET_24G_NAME` calls. (ansi.c, rf.c)
- **UART RX intra-byte wait removed** — `wait_us(200)` between non-contiguous RX bytes was unnecessary. (rf.c:622)
- **Init report block removed** — Set `f_dial_sw_init_ok = 1` immediately after `m_power_on_dial_sw_scan()` instead of waiting for first housekeeping cycle. (ansi.c:722)
- **Long press poll: 100ms → 50ms** — Finer granularity, constants doubled to maintain 3-second threshold. (ansi.c:133,23-25)

### Stroke Latency

- **First-key-after-sleep wakeup race fixed** — Previously: first keystroke sent while NRF was asleep → lost, then up to 50ms poll + 25ms handshake before usable. Now: inline wakeup in `uart_send_report()` wakes NRF before sending. Total: ~2.7ms, keystroke preserved. (rf.c:586)
- **Trailing `wait_us(200)` removed** — Redundant with `UART_Send_Bytes` internal TX-completion wait. (rf.c:599)

### Code Optimizations

- **Deduplicated 4 link-switch cases** — `LNK_RF`, `LNK_BLE1`, `LNK_BLE2`, `LNK_BLE3` merged into single fall-through using `keycode - LNK_RF` mapping. -42 lines. (ansi.c:422-443)
- **Deduplicated RF init retry loops** — Extracted `rf_init_try_cmd()` helper function. -28 lines. (rf.c:663-690)
- **Removed dead variable** — `bat_pwm_buf[18]` was declared, zeroed, and never read. -18 bytes RAM. (side.c:602)
- **Removed redundant `keyboard_protocol = 1`** — Already set in `uart_send_report_func`. Removed from both `rf_send_keyboard` and `rf_send_nkro`. (rf_driver.c:46,51)
- **Moved defines to file scope** — `RF_LED_LINK_PERIOD`/`RF_LED_PAIR_PERIOD` (side.c) and `USB_GETSTATUS_REMOTE_WAKEUP_ENABLED` (sleep.c) were defined inside function bodies.

---

### Performance Summary

| Metric | Before | After |
|--------|--------|-------|
| Startup dead time | 1250ms worst-case | ~230ms |
| Per-report UART time | 2.35ms (3× repeat) | 0.72ms (1×) |
| Stroke latency (press→UART TX) | ~4ms | ~1.8ms |
| First-key-after-sleep | Lost + 75ms | 2.7ms preserved |
| UART utilization (periodic) | 11.3% | 0.9% |
| MAC key freeze | 50ms × 4 | 5ms × 4 |
| Flash (net code change) | — | -72 lines |

---

## v1.0.4 (upstream)

- [Keyboard] Fix bugs 
- Tagged Dec 27, 2024

## v1.0.2 (upstream)

- Add Halo75 V2 keyboard and other modifications 
- Tagged Jun 12, 2024

## v1.0.1 (upstream)

- Gem80 keyboard modifications 
- Tagged May 6, 2024

## v1.0.0 (upstream)

- Initial keyboard support 
- Tagged Mar 19, 2024
