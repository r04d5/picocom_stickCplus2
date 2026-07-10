#include "autobaud.h"
#include "uart_manager.h"
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

// Time each candidate rate is listened to before moving on.
#define SWEEP_WINDOW_MS 350
// A rate qualifies only if it produced enough bytes and they are mostly
// printable -- a wrong rate yields framing garbage with a low printable ratio.
#define SWEEP_MIN_BYTES 8
#define SWEEP_MIN_RATIO_PCT 70

int ab_method = AB_METHOD_HARDWARE;

static bool sweep_running = false;
static int sweep_idx = 0;
static uint32_t sweep_step_start = 0;
static uint32_t cur_total = 0;
static uint32_t cur_print = 0;
static uint32_t best_baud = 0;
static uint32_t best_score = 0;

static bool is_readable(uint8_t b) {
    return (b >= 32 && b <= 126) || b == '\r' || b == '\n' || b == '\t';
}

static void begin_window(int idx) {
    cur_total = 0;
    cur_print = 0;
    init_uart(baud_rates[idx]);
    uart_flush_input(UART_NUM_1);
    sweep_step_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static void finalize_window(int idx) {
    // Score the just-finished rate.
    if (cur_total >= SWEEP_MIN_BYTES) {
        uint32_t ratio_pct = (cur_print * 100) / cur_total;
        if (ratio_pct >= SWEEP_MIN_RATIO_PCT && cur_print > best_score) {
            best_score = cur_print;
            best_baud = baud_rates[idx];
        }
    }
    printf("[SWEEP] %u bps: %u/%u printable\n",
           (unsigned int)baud_rates[idx], (unsigned int)cur_print, (unsigned int)cur_total);
}

void start_baud_sweep() {
    // Pause the async RX task so this routine owns the UART.
    uart_rx_enabled = false;
    vTaskDelay(pdMS_TO_TICKS(20));

    ab_method = AB_METHOD_SWEEP;
    ab_state = AB_STATE_RUNNING;
    ab_detected_baud = 0;
    sweep_running = true;
    sweep_idx = 0;
    best_baud = 0;
    best_score = 0;
    begin_window(0);
    printf("[SWEEP] Started passive baud sweep\n");
}

static void finish_sweep() {
    sweep_running = false;

    // Restore the standard config and resume the async RX task.
    if (best_baud > 0) {
        ab_detected_baud = best_baud;
        // Point the live config at the winning rate so applying is seamless.
        current_baud = best_baud;
        for (int i = 0; i < 8; i++) {
            if (baud_rates[i] == best_baud) {
                baud_idx = i;
                break;
            }
        }
    }
    init_uart(current_baud);
    uart_rx_enabled = true;

    if (best_baud > 0) {
        ab_state = AB_STATE_SUCCESS;
        printf("[SWEEP] Success: %u bps\n", (unsigned int)best_baud);
    } else {
        ab_state = AB_STATE_FAILED;
        printf("[SWEEP] Failed: no rate produced readable text\n");
    }
}

void baud_sweep_step() {
    if (!sweep_running) {
        return;
    }

    // Accumulate whatever bytes are currently available at this rate.
    uint8_t buf[128];
    int len = uart_read_bytes(UART_NUM_1, buf, sizeof(buf), 0);
    for (int i = 0; i < len; i++) {
        cur_total++;
        if (is_readable(buf[i])) {
            cur_print++;
        }
    }

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - sweep_step_start >= SWEEP_WINDOW_MS) {
        finalize_window(sweep_idx);
        sweep_idx++;
        if (sweep_idx >= 8) {
            finish_sweep();
        } else {
            begin_window(sweep_idx);
        }
    }
}

void cancel_baud_sweep() {
    if (!sweep_running) {
        return;
    }
    sweep_running = false;
    init_uart(current_baud);
    uart_rx_enabled = true;
    printf("[SWEEP] Cancelled\n");
}

int baud_sweep_current_idx() {
    return sweep_idx;
}

uint32_t baud_sweep_best_baud() {
    return best_baud;
}
