# Changelog

All notable changes to the optimized Air96 V2 custom firmware fork.

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
