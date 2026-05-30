# Changelog

All notable changes to the optimized Air96 V2 custom firmware fork.

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
