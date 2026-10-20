# Changelog

All notable changes to the optimized Air96 V2 custom firmware fork.

## v3.2.4 (2026-10-20)
*Wireless responsiveness and rollover fixes.*

- **Nonblocking radio reset**: Preserve the radio reset timing while allowing keyboard scanning between reset steps, removing the 200 ms main-loop stall.
- **Modifier tracking after idle**: Retain wireless bitmap tracking so modifier changes still update held overflow keys after an idle period.
- **Higher keycodes during rollover**: Reserve the six byte-report slots for keycodes outside the radio bitmap range. Keep held bitmap keys on their endpoint to avoid false releases during report transitions. The existing protocol still limits simultaneous higher keycodes to six.
- **Regression coverage**: Add host-compiled checks for report contents, idle modifiers, releases, overflow recovery, and reset timing, and run them in build CI.
- **Validation**: Default and VIA builds and regression checks pass. Physical latency and delivery during a radio reboot remain unverified.

## v3.2.3 (2026-06-02)
*Incremental I2C flush — eliminates keystroke hiccups during rapid typing.*

- **Chunked RGB LED writes**: The IS31FL3733 PWM flush now writes one 16-byte I2C chunk per main-loop iteration (~0.2 ms) instead of all 24 transactions in one blocking call (~4 ms). The key matrix scan runs between chunks, so fast keypresses that previously landed inside the long I2C window are no longer missed. Fixes the characteristic `kkkkkk(lag)kkkkkk` stutter seen during rapid typing.

## v3.2.2 (2026-06-02)
*Bug-fix pass — boot NumLock reliability, link-key cross-release, dial latency.*

- **No more double input at boot**: The auto-NumLock one-shot now waits 2 s after USB enumeration, requires a 200 ms idle window, and holds the press for 50 ms. The old 1 ms press was both too short for some hosts to register and could land on top of the user's first keypress, toggling host NumLock mid-typing. The host's current NumLock state is now also checked, so the toggle is skipped if it's already on.
- **Link keys no longer cross-fire**: Holding `LNK_BLE1` and tapping `LNK_BLE2` no longer switches the keyboard to BLE1 on BLE1's release — only the key that was actually pressed now triggers the switch on release.
- **Faster dial switching**: A single physical dial detent previously called `m_break_all_key()` up to three times (~60 ms of blocking). The redundant calls in the SYS-mode branches are gone, since the first-tick release already covers the held-key case.
- **Safer EEPROM load**: The OOB clamps on `side_mode/light/speed/rgb/colour` now run on every boot, not just subsequent ones — so a stale flag from a prior firmware flash can't push the animation into a bad state.

## v3.2.1 (2026-06-01)
*Sleep/wake regression fixes — side LED blinking and wake-key corruption.*

- **Side LEDs no longer blink when the PC is off**: The v3.2.0 "wakeup timeout fallback" woke the board ~500ms after every sleep with no real trigger, causing it to re-power the LEDs, fail to wake the off host, and re-suspend — an endless off/on cycle visible as blinking side lights. The board now stays asleep (LEDs and NRF powered down) until a genuine wake trigger: a keypress, or in USB mode the host actually resuming the bus.
- **Wake keypress no longer corrupted**: On a local key-wake, the keypress that triggered the wake is already in flight; the v3.2.0 unconditional `m_break_all_key()` released it mid-hold. Key state is now only flushed on a host-driven (USB resume) wake, eliminating the dropped/doubled input on the first key after sleep.

## v3.2.0 (2026-05-31)
*Stability overhaul — sleep/wake, mode switching, LED, and protocol fixes.*

- **Fixed double keypresses**: Raised key debounce from 1ms to 5ms — the old value was too short for mechanical switch bounce, causing phantom repeat inputs on fast taps.
- **Right-side LEDs stay rainbow in wired mode**: Battery indicator (orange breathing) now only activates in wireless/battery mode. Wired USB mode always shows the selected RGB animation.
- **No more half-asleep lockup**: Wireless sleep requests are now rejected when USB is active. Added a 500ms wakeup timeout fallback so the keyboard can't get stuck in a half-asleep state.
- **Cleaner wake from sleep**: All held keys are cleared on wake (not just USB mode), preventing stale keypresses. USB suspend debounce and disconnect timers reset properly after wakeup.
- **Faster USB wakeup**: Reduced host wakeup retry loop from 500ms to 250ms max.
- **Safer mode switching**: Switching between USB/BT/RF now resets all sync counters, link timers, and disconnect state. The USB mode key no longer sends redundant commands when already in USB mode.
- **NRF sync reliability**: Short 3-byte ACKs from the wireless module now properly clear the sync-lost counter, preventing unnecessary NRF resets.
- **Buffer overflow guards**: Added bounds checks on UART send functions as defense-in-depth.
- **Volatile correctness**: Fixed extern declarations that were missing volatile qualifiers, preventing potential issues with link-time optimization.
- **Battery indicator auto-dismiss**: The manual battery percent display (BAT_SHOW key) now auto-clears after 10 seconds instead of staying on forever.
- **Smoother colour cycling**: Fixed the side LED colour control wrapping past index 0 and skipping colours.
- **Reduced blocking**: Bluetooth pairing command retries reduced from 100ms to 60ms, and USB mode key no longer blocks when already in USB mode.

## v3.0.8 (2026-05-26)
*Wired latency, host driver fixes, and RF init timeouts.*

- **Faster wired typing**: Set USB polling to 1000Hz (1ms) and switched to per-key debouncing that registers keypresses on the first scan — zero added latency.
- **Fixed mode switching lockup**: The USB host driver is no longer overwritten when booting in wireless mode, so switching back to wired works reliably.
- **Smarter RF startup**: Added a proper response timeout when initializing the wireless module, avoiding redundant command spam and fixing startup communication races.

## v3.0.7 (2026-05-26)
*Safety hardening, wireless protocol checks, and animation fixes.*

- **Auto-sleep works in all modes**: Sleep operations (USB suspend, wireless disconnect, connection timeout) now execute correctly regardless of the auto-sleep setting.
- **Safer wireless packets**: Added length checks on incoming wireless status data to guard against corrupted packets causing crashes or stale readings.
- **Smoother LED animations**: Fixed a math bug in the circular color cycling that could glitch at certain speeds.
- **Battery indicator no longer gets stuck**: The right-side orange breathing animation now shows once when fully charged, then turns off instead of running forever while plugged in.
- **Safer settings recovery**: Side LED color and mode settings loaded from memory are validated and clamped to safe values, preventing random flashes if the stored config is corrupted.

## v3.0.6 (2026-05-26)
*Comprehensive bug-fixing, C code smells refactoring, and memory footprint optimizations.*

- **Fixed Startup & Wake Latency**: Embedded serial packet parsing directly inside command wait loops, eliminating 30+ms blocking timeouts at boot and wakeup.
- **Fixed USB Suspend Power Compliance**: Hardened the sleep handler to force LED and NRF power-downs during USB suspend, satisfying standard USB low-power specs (<2.5mA) to prevent laptop battery drain.
- **Fixed IDE Undefined Types**: Added proper `<stdint.h>` type inclusions to solve editor syntax parser warnings in custom side LED headers.
- **Optimized Memory Footprint**: Refactored battery LED status lookup logic to use clean dynamic arrays instead of nested `if` statements, shrinking the binary by **96 bytes**.
- **Stylized Codebases**: Resolved all single-line brace omissions and implicit boolean type conversions across the entire source tree.

## v3.0.5 (2026-05-26)
*Reliable host synchronization improvements.*

- **Added Deferred NumLock Sync**: Moved NumLock auto-on signaling to a deferred task runner with a 2.5-second hold delay post-enumeration. This ensures the Host OS registers the state perfectly instead of silently dropping the early command during boot.

## v3.0.4 (2026-05-26)
*Automated QA and codebase linting pass.*

- **Added CI Lint Workflow**: Introduced automated `cppcheck` (blocking) and `clang-tidy` (advisory) GitHub Actions to keep future firmware iterations robust.
- **Refactored Function Linkages**: Restructured scope boundaries by making 13 internal protocol functions `static` to prevent namespace pollution.

## v3.0.3 (2026-05-26)
*Main loop responsiveness tweaks.*

- **Optimized Loop Latency**: Eliminated an idle 2ms wait in the keyboard’s main task runner, allowing concurrent execution of wireless state syncs alongside standard USB polling.

## v3.0.2 (2026-05-26)
*Deep-dive wireless subsystem audit and latency tuning.*

- **Fixed Sleep/Wake Race Condition**: Prevented report buffer commands from prematurely clearing wakeup indicators, ensuring the first keypress always wakes the Host PC.
- **Fixed Buffer Overread Guard**: Added robust bounds checking on incoming RF serial packets to protect against malformed packet buffer overflows.
- **Optimized USB Battery Saving**: Scaled down periodic wireless status pings from 5 pings/sec to 1 ping/2sec when connected via USB wire, saving 2-3mA on battery.
- **Tuned Typing Dispatch**: Reduced trailing idle reports and adjusted key-repeat latency intervals from 200ms to 100ms for more responsive typing.

## v3.0.1 (2026-05-26)
*Keyboard protocol correctness fixes.*

- **Fixed Modifier Lag**: Synced key modifiers to dispatch immediately on change under NKRO wireless mode to eliminate modifier key lag.
- **Fixed Startup Order**: Gated initial dial switch scans until the core USB host driver is fully initialized to prevent boot crashes.

## v3.0.0 (2025-05-25)
*Initial high-performance optimization audit.*

- **Slashed Boot Delays**: Reduced blind boot-up wait cycles from 500ms to 100ms.
- **Instant OS-Shortcut Registration**: Slashed key chord delays in macOS Spotlight and screenshot chording from 50ms to 5ms for instantaneous registrations.
- **Corrected Battery Charging Indicators**: Kept the side breathing LED active during active charging cycles instead of falsely overriding charging state data.
