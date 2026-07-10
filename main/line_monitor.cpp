#include "line_monitor.h"
#include "uart_manager.h"
#include <driver/gpio.h>

// A sustained low longer than this is treated as an anomaly. It is orders of
// magnitude above a byte frame (a byte at 9600 baud is ~1 ms) and above normal
// inter-frame gaps, so ordinary traffic never trips it.
#define LINE_STUCK_MS 250

static bool tracking = false;
static uint32_t low_start = 0;
static bool stuck = false;

void line_monitor_reset() {
    tracking = false;
    stuck = false;
    low_start = 0;
}

void line_monitor_update(uint32_t now_ms) {
    int lvl = gpio_get_level(get_grove_rx_pin());
    if (lvl == 0) {
        if (!tracking) {
            tracking = true;
            low_start = now_ms;
        } else if (now_ms - low_start >= LINE_STUCK_MS) {
            stuck = true;
        }
    } else {
        // Any high sample means the line is alive (idle or mid-traffic).
        tracking = false;
        stuck = false;
    }
}

bool line_stuck_low() {
    return stuck;
}
