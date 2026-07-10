#pragma once

#include <stdint.h>

// Auto-Baudrate State
enum AutoBaudState {
    AB_STATE_IDLE,
    AB_STATE_RUNNING,
    AB_STATE_SUCCESS,
    AB_STATE_FAILED
};
extern AutoBaudState ab_state;
extern uint32_t ab_detected_baud;
extern uint32_t ab_start_time;

// Function prototypes
uint32_t snap_to_standard_baud(uint32_t calculated);
uint32_t get_rtc_fast_freq();
void start_autobaud_detection();
void stop_autobaud_detection();
