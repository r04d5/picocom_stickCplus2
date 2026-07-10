#pragma once

#include <stdint.h>
#include <stdbool.h>

// Cardputer matrix keyboard driver (ESP-IDF port of the M5Cardputer
// IOMatrix reader). Only the classic M5Cardputer wires its keyboard as a
// 74HC138-demuxed GPIO matrix; the CardputerADV uses a TCA8418 I2C chip and
// is not handled here. keyboard_init() detects the board and no-ops on any
// other hardware, so keyboard_poll() simply returns 0 there.

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KB_CHAR,       // printable ASCII in kb_event_t.ch
    KB_ENTER,
    KB_BACKSPACE,
    KB_TAB,
    KB_ESC,
    KB_UP,
    KB_DOWN,
    KB_LEFT,
    KB_RIGHT,
} kb_keytype_t;

typedef struct {
    kb_keytype_t type;
    char ch;       // valid when type == KB_CHAR
} kb_event_t;

// Configure the matrix GPIOs. Safe to call unconditionally; it only claims
// pins when running on a classic M5Cardputer.
void keyboard_init(void);

// True when a supported keyboard was detected by keyboard_init().
bool keyboard_present(void);

// Scan the matrix once and report keys that were newly pressed since the
// previous call (edge-triggered). Returns the number of events written, up
// to max_events. Returns 0 when no keyboard is present.
int keyboard_poll(kb_event_t *events, int max_events);

#ifdef __cplusplus
}
#endif
