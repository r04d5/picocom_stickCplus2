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

// Detection method backing the current AB_STATE_RUNNING run.
enum AutoBaudMethod {
    AB_METHOD_HARDWARE = 0, // ESP32 pulse-timing subsystem (needs clean edges)
    AB_METHOD_SWEEP         // passive brute-force sweep (needs readable text)
};
extern int ab_method;

// Function prototypes
uint32_t snap_to_standard_baud(uint32_t calculated);
uint32_t get_rtc_fast_freq();
void start_autobaud_detection();
void stop_autobaud_detection();

// Passive baud sweep: reconfigures the UART across the standard rates and
// scores each by how much readable ASCII the target emits, picking the rate
// with the most printable text. Works on any console/boot-log output even when
// the hardware pulse method cannot lock (no clean edges).
void start_baud_sweep();
void baud_sweep_step();          // advance the stepped sweep from the main loop
void cancel_baud_sweep();        // abort mid-sweep and restore normal RX
int baud_sweep_current_idx();    // rate index currently under test (0..7)
uint32_t baud_sweep_best_baud(); // best candidate so far (0 if none)
