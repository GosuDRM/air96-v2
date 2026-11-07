/* Copyright 2025 @ GosuDRM
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* Halt the core (WFI) whenever the main thread sleeps in wait_ms(). Clocks and
   peripherals keep running; the ChibiOS alarm or any IRQ resumes execution. */
#define CORTEX_ENABLE_WFI_IDLE TRUE

#include_next <chconf.h>
