#pragma once

#include <stdint.h>
#include <stdbool.h>

// Parser fuzzer.
// Sends deliberately malformed payloads (oversized fields, invalid checksums,
// control-byte floods, delimiter storms, broken frames) to the target and
// watches the RX line for liveness. A target that was answering and then goes
// silent under fuzzing is a strong signal of a parser hang / buffer overflow /
// DoS -- i.e. an insecure parser (guide sections 8.3 / 8.7 / 8.8, Phase 3).

enum FuzzHealth {
    FZ_UNKNOWN = 0,   // no RX baseline yet (target never spoke)
    FZ_RESPONSIVE,    // target replied recently
    FZ_SILENT,        // brief silence -- warning
    FZ_HANG           // was responsive, now silent for long -> possible crash/DoS
};

extern int fuzz_case_idx;
extern bool fuzzer_active;
extern uint32_t fuzz_sent_count;
extern const int FUZZ_CASE_COUNT;

const char* fuzz_case_name(int idx);

void fuzzer_reset();
void fuzzer_start();
void fuzzer_stop();
void run_fuzzer_tick();

// Called by the main loop whenever RX activity is observed.
void fuzzer_note_rx(uint32_t now_ms);

int fuzzer_health();
uint32_t fuzzer_ms_since_rx();
