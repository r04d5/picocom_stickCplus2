#include "fuzzer.h"
#include "uart_manager.h"
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdio.h>

// Health thresholds (ms since last observed RX byte).
#define FZ_SILENT_MS 1500
#define FZ_HANG_MS   5000

// Interval between fuzz payloads.
#define FUZZ_INTERVAL_MS 250

int fuzz_case_idx = 0;
bool fuzzer_active = false;
uint32_t fuzz_sent_count = 0;

static const char* case_names[] = {
    "NMEA field overflow",
    "AT command overflow",
    "NMEA bad checksum",
    "Ctrl-byte flood",
    "Delimiter storm",
    "Modbus broken frame"
};
const int FUZZ_CASE_COUNT = sizeof(case_names) / sizeof(case_names[0]);

static uint32_t last_spam_time = 0;

// Liveness tracking
static volatile uint32_t last_rx_ms = 0;
static volatile bool ever_responded = false;
static bool have_rx_baseline = false;

const char* fuzz_case_name(int idx) {
    if (idx < 0 || idx >= FUZZ_CASE_COUNT) {
        return "?";
    }
    return case_names[idx];
}

void fuzzer_reset() {
    fuzzer_active = false;
    fuzz_sent_count = 0;
    last_spam_time = 0;
    have_rx_baseline = false;
    ever_responded = false;
    last_rx_ms = 0;
}

void fuzzer_start() {
    fuzzer_active = true;
    fuzz_sent_count = 0;
    last_spam_time = 0;
    // Reset the liveness baseline: any silence measured from now on is
    // attributable to the fuzzing we are about to do.
    ever_responded = false;
    have_rx_baseline = false;
    last_rx_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void fuzzer_stop() {
    fuzzer_active = false;
}

void fuzzer_note_rx(uint32_t now_ms) {
    last_rx_ms = now_ms;
    have_rx_baseline = true;
    if (fuzzer_active) {
        ever_responded = true;
    }
}

// Builds the malformed payload for the given case into buf (capacity cap).
// Returns the byte length produced.
static int build_payload(int idx, uint8_t* buf, int cap) {
    switch (idx) {
        case 0: {
            // Oversized NMEA field: a giant field to probe fixed-size buffers.
            int n = 0;
            const char* head = "$GPGGA,";
            n += snprintf((char*)buf + n, cap - n, "%s", head);
            int fill = 220;
            for (int i = 0; i < fill && n < cap - 8; i++) buf[n++] = 'A';
            n += snprintf((char*)buf + n, cap - n, ",,,,*00\r\n");
            return n;
        }
        case 1: {
            // Oversized AT command line.
            int n = 0;
            n += snprintf((char*)buf + n, cap - n, "AT");
            int fill = 200;
            for (int i = 0; i < fill && n < cap - 4; i++) buf[n++] = 'A';
            n += snprintf((char*)buf + n, cap - n, "\r\n");
            return n;
        }
        case 2: {
            // Well-formed sentence but deliberately wrong checksum.
            const char* s = "$GPRMC,123519,A,4807.038,N,01131.000,E*00\r\n";
            int n = (int)strlen(s);
            if (n > cap) n = cap;
            memcpy(buf, s, n);
            return n;
        }
        case 3: {
            // Control-byte flood: NUL, XON, XOFF, 0xFF -- flow-control abuse and
            // non-printable bytes that naive parsers mishandle.
            const uint8_t pat[4] = {0x00, 0x11, 0x13, 0xFF};
            int n = 0;
            for (int i = 0; i < 32 && n < cap; i++) buf[n++] = pat[i & 3];
            return n;
        }
        case 4: {
            // Delimiter storm: repeated framing characters with empty fields.
            const char* s = "$$$$,,,,****\r\n\r\n,,;;$*,*\r\n";
            int n = (int)strlen(s);
            if (n > cap) n = cap;
            memcpy(buf, s, n);
            return n;
        }
        case 5:
        default: {
            // Modbus frame claiming a huge byte count but truncated, with a
            // bad CRC -- tests length validation and CRC checking.
            const uint8_t frame[8] = {0x01, 0x03, 0xFF, 0x00, 0x0A, 0x00, 0x00, 0x00};
            int n = (int)sizeof(frame);
            if (n > cap) n = cap;
            memcpy(buf, frame, n);
            return n;
        }
    }
}

void run_fuzzer_tick() {
    if (!fuzzer_active) {
        return;
    }
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now_ms - last_spam_time < FUZZ_INTERVAL_MS) {
        return;
    }

    uint8_t buf[300];
    int len = build_payload(fuzz_case_idx, buf, sizeof(buf));
    if (len > 0) {
        uart_write_bytes(UART_NUM_1, (const char*)buf, len);
        tx_bytes += len;
        fuzz_sent_count++;
    }
    last_spam_time = now_ms;
}

uint32_t fuzzer_ms_since_rx() {
    if (!have_rx_baseline) {
        return 0xFFFFFFFF;
    }
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return now_ms - last_rx_ms;
}

int fuzzer_health() {
    if (!have_rx_baseline) {
        return FZ_UNKNOWN;
    }
    uint32_t dt = fuzzer_ms_since_rx();
    if (dt < FZ_SILENT_MS) {
        return FZ_RESPONSIVE;
    }
    if (dt < FZ_HANG_MS) {
        return FZ_SILENT;
    }
    // Long silence: a regression from a previously-responsive target is the
    // interesting finding; a target that never answered is just inconclusive.
    return ever_responded ? FZ_HANG : FZ_SILENT;
}
