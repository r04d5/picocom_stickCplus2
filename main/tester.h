#pragma once

#include <stdint.h>
#include <stdbool.h>

// Self-test harness.
// Exercises every analysis capability end-to-end through the real UART: it
// transmits a known vector for each feature and checks that the corresponding
// module counter reacted. Requires the bytes to loop back -- either wire TX->RX
// on the same device (loopback jumper) or connect a second unit with ECHO ON.

enum TestResult {
    TR_PENDING = 0,
    TR_PASS,
    TR_FAIL
};

extern bool tester_active;

int tester_step_count();
const char* tester_step_name(int i);
int tester_step_result(int i);   // TestResult
int tester_current_step();
int tester_passed();

void tester_reset();
void tester_start();
void tester_stop();
void tester_step();              // advance the run from the main loop
