#include "spammer.h"
#include "uart_manager.h"
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

int selected_macro_idx = 0;
bool spammer_active = false;
uint32_t spam_sent_count = 0;

// Predefined Macros Configurations
Macro macros[] = {
    {"U-Boot Interrupt", (const uint8_t*)" ", 1, 15},                // Spam Spacebar every 15ms
    {"AT Attention", (const uint8_t*)"AT\r\n", 4, 500},               // AT ping every 500ms
    {"Modbus RTU Poll", (const uint8_t*)"\x01\x03\x00\x00\x00\x01\x84\x0A", 8, 1000}, // Read reg 0 every 1000ms
    {"GPS Query", (const uint8_t*)"$EGPQ,RMC*3C\r\n", 15, 2000}       // Query NMEA GPS every 2000ms
};

const int MACRO_COUNT = sizeof(macros) / sizeof(macros[0]);
static uint32_t last_spam_time = 0;

void run_spammer_tick() {
    if (!spammer_active) {
        return;
    }

    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    Macro &m = macros[selected_macro_idx];

    if (now_ms - last_spam_time >= m.interval_ms) {
        uart_write_bytes(UART_NUM_1, (const char*)m.data, m.len);
        tx_bytes += m.len;
        spam_sent_count++;
        last_spam_time = now_ms;
    }
}
