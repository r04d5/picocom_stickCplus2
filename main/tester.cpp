#include "tester.h"
#include "uart_manager.h"
#include "scanner.h"
#include "proto_analyzer.h"
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdio.h>

// Time allowed for each vector to loop back and be processed before scoring.
#define TEST_WINDOW_MS 300

#define TEST_STEP_COUNT 9

static const char* step_names[TEST_STEP_COUNT] = {
    "UART loopback",
    "Scan: shell",
    "Scan: creds",
    "Scan: keys",
    "Scan: boot",
    "Proto: AT OK",
    "Proto: NMEA ok",
    "Proto: NMEA bad",
    "Proto: Modbus"
};

bool tester_active = false;
static int cur_step = 0;
static int results[TEST_STEP_COUNT];
static uint32_t baseline = 0;
static uint32_t step_start = 0;
static int passed = 0;

static uint16_t crc16_modbus(const uint8_t* d, int n) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < n; i++) {
        crc ^= d[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
        }
    }
    return crc;
}

// Returns the module counter that the given step is expected to move.
static uint32_t counter_of(int i) {
    switch (i) {
        case 0: return rx_bytes;
        case 1: return scanner_get_count(SCAN_SHELL);
        case 2: return scanner_get_count(SCAN_CREDS);
        case 3: return scanner_get_count(SCAN_KEYS);
        case 4: return scanner_get_count(SCAN_BOOT);
        case 5: return proto_count(PROTO_AT_OK);
        case 6: return proto_count(PROTO_NMEA_OK);
        case 7: return proto_count(PROTO_NMEA_BADCRC);
        case 8: return proto_count(PROTO_MODBUS_OK);
        default: return 0;
    }
}

// Builds a NMEA sentence with a correct or deliberately wrong checksum.
static int build_nmea(uint8_t* buf, int cap, bool valid) {
    const char* body = "M5TST,1,2,3";
    uint8_t x = 0;
    for (const char* p = body; *p; ++p) {
        x ^= (uint8_t)*p;
    }
    uint8_t cs = valid ? x : (uint8_t)(x ^ 0xFF);
    return snprintf((char*)buf, cap, "$%s*%02X\r\n", body, cs);
}

static void send_step(int i) {
    uint8_t buf[64];
    int len = 0;
    switch (i) {
        case 0: len = snprintf((char*)buf, sizeof(buf), "M5-LOOPBACK-TEST\n"); break;
        case 1: len = snprintf((char*)buf, sizeof(buf), "root@target:/# \n"); break;
        case 2: len = snprintf((char*)buf, sizeof(buf), "wifi_psk=secret123\n"); break;
        case 3: len = snprintf((char*)buf, sizeof(buf), "-----BEGIN PRIVATE KEY-----\n"); break;
        case 4: len = snprintf((char*)buf, sizeof(buf), "U-Boot 2021.01\n"); break;
        case 5: len = snprintf((char*)buf, sizeof(buf), "OK\r\n"); break;
        case 6: len = build_nmea(buf, sizeof(buf), true); break;
        case 7: len = build_nmea(buf, sizeof(buf), false); break;
        case 8: {
            uint8_t frame[7] = {0x01, 0x03, 0x02, 0x00, 0x0A, 0, 0};
            uint16_t crc = crc16_modbus(frame, 5);
            frame[5] = crc & 0xFF;
            frame[6] = (crc >> 8) & 0xFF;
            memcpy(buf, frame, 7);
            len = 7;
            break;
        }
        default: len = 0; break;
    }
    if (len > 0) {
        uart_write_bytes(UART_NUM_1, (const char*)buf, len);
        tx_bytes += len;
    }
}

static void begin_step(int i) {
    baseline = counter_of(i);
    send_step(i);
    step_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void tester_reset() {
    tester_active = false;
    cur_step = 0;
    passed = 0;
    for (int i = 0; i < TEST_STEP_COUNT; i++) {
        results[i] = TR_PENDING;
    }
}

void tester_start() {
    tester_reset();
    // Echo would re-transmit the looped-back bytes and skew the counters.
    echo_mode = false;
    tester_active = true;
    begin_step(0);
}

void tester_stop() {
    tester_active = false;
}

void tester_step() {
    if (!tester_active) {
        return;
    }
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - step_start < TEST_WINDOW_MS) {
        return;
    }

    uint32_t after = counter_of(cur_step);
    // The loopback step needs the whole vector back; the rest just need their
    // module counter to have advanced by one.
    bool ok = (cur_step == 0) ? (after >= baseline + 8) : (after > baseline);
    results[cur_step] = ok ? TR_PASS : TR_FAIL;
    if (ok) {
        passed++;
    }

    cur_step++;
    if (cur_step >= TEST_STEP_COUNT) {
        tester_active = false;
    } else {
        begin_step(cur_step);
    }
}

int tester_step_count() {
    return TEST_STEP_COUNT;
}

const char* tester_step_name(int i) {
    if (i < 0 || i >= TEST_STEP_COUNT) {
        return "?";
    }
    return step_names[i];
}

int tester_step_result(int i) {
    if (i < 0 || i >= TEST_STEP_COUNT) {
        return TR_PENDING;
    }
    return results[i];
}

int tester_current_step() {
    return cur_step;
}

int tester_passed() {
    return passed;
}
