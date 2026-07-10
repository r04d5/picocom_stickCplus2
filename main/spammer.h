#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Macro configuration structure
struct Macro {
    const char* name;
    const uint8_t* data;
    size_t len;
    uint32_t interval_ms;
};

// Spammer / Macros global state variables
extern int selected_macro_idx;
extern bool spammer_active;
extern uint32_t spam_sent_count;
extern const int MACRO_COUNT;
extern Macro macros[];

// Function prototypes
void run_spammer_tick();
