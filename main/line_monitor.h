#pragma once

#include <stdint.h>
#include <stdbool.h>

// Line anomaly monitor.
// A healthy idle TTL UART line rests HIGH. A line held LOW for far longer than
// any legitimate frame indicates a short, a jammed transmitter or a continuous
// break condition -- a line-level DoS (guide section 8.8). This samples the RX
// pin level and flags a sustained stuck-low.

void line_monitor_reset();

// Sample the RX pin. Call periodically from the main loop.
void line_monitor_update(uint32_t now_ms);

bool line_stuck_low();
