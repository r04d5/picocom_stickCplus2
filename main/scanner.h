#pragma once

#include <stdint.h>
#include <stdbool.h>

// Passive security scanner.
// Consumes the raw UART RX stream byte-by-byte, reassembles lines and flags
// content that indicates a common UART exposure: interactive shells / auth
// prompts, leaked credentials, private keys and bootloader interruption windows.

enum ScanCategory {
    SCAN_SHELL = 0,   // interactive console / auth prompt exposed
    SCAN_CREDS,       // credential material in plaintext
    SCAN_KEYS,        // private keys / API keys
    SCAN_BOOT,        // bootloader banner / interruptible boot
    SCAN_CATEGORY_COUNT
};

void scanner_reset();
void scanner_feed(char c);

uint32_t scanner_get_count(int category);
uint32_t scanner_get_total();
const char* scanner_category_name(int category);

// Last matched line (already truncated for the display) and its category,
// or an empty string / -1 when nothing has matched yet.
const char* scanner_last_hit();
int scanner_last_category();
