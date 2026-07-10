#include "scanner.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define SCAN_LINE_MAX 160
#define SCAN_SNIP_MAX 40

static char line_buf[SCAN_LINE_MAX + 1];
static int line_len = 0;

static uint32_t counts[SCAN_CATEGORY_COUNT];
static uint32_t total_hits = 0;
static char last_snip[SCAN_SNIP_MAX + 1] = "";
static int last_cat = -1;

// Pattern tables (matched case-insensitively as substrings). Each list is
// NULL-terminated. Keep these lowercase.
static const char* pat_shell[] = {
    "login:", "password:", "root@", "busybox", "/bin/sh", "/bin/ash",
    "uid=0", "command not found", "~ #", "console:", NULL
};
static const char* pat_creds[] = {
    "psk=", "psk:", "passphrase", "wpa_passphrase", "ssid=", "secret=",
    "secret:", "token=", "token:", "bearer ", "api_key", "apikey",
    "pass=", "pwd=", NULL
};
static const char* pat_keys[] = {
    "-----begin", "ssh-rsa", "ssh-ed25519", "private key",
    "aws_secret", "aws_access", NULL
};
static const char* pat_boot[] = {
    "u-boot", "hit any key", "bootloader", "starting kernel",
    "autoboot", "zephyr", "reset cause", "bootargs", NULL
};

static const char* category_names[SCAN_CATEGORY_COUNT] = {
    "SHELL", "CREDS", "KEYS", "BOOT"
};

void scanner_reset() {
    line_len = 0;
    line_buf[0] = '\0';
    total_hits = 0;
    last_cat = -1;
    last_snip[0] = '\0';
    for (int i = 0; i < SCAN_CATEGORY_COUNT; i++) {
        counts[i] = 0;
    }
}

static bool list_matches(const char* const* list, const char* low) {
    for (int i = 0; list[i] != NULL; i++) {
        if (strstr(low, list[i]) != NULL) {
            return true;
        }
    }
    return false;
}

// Heuristic for a bare shell prompt (e.g. "/ #", "=> ", "$"). Reduces false
// positives from the substring tables by looking at how the line terminates.
static bool looks_like_prompt(const char* s, int len) {
    int end = len;
    while (end > 0 && s[end - 1] == ' ') {
        end--;
    }
    if (end == 0) {
        return false;
    }
    char last = s[end - 1];
    if (last == '#' || last == '$') {
        return true;
    }
    if (end >= 2 && s[end - 2] == '=' && s[end - 1] == '>') {
        return true; // U-Boot "=>" prompt
    }
    return false;
}

static void record_hit(int category, const char* raw) {
    counts[category]++;
    total_hits++;
    last_cat = category;
    snprintf(last_snip, sizeof(last_snip), "%s", raw);
}

static void scan_line() {
    if (line_len == 0) {
        return;
    }
    line_buf[line_len] = '\0';

    char low[SCAN_LINE_MAX + 1];
    for (int i = 0; i < line_len; i++) {
        low[i] = (char)tolower((unsigned char)line_buf[i]);
    }
    low[line_len] = '\0';

    bool hit[SCAN_CATEGORY_COUNT] = {false, false, false, false};
    if (list_matches(pat_shell, low) || looks_like_prompt(line_buf, line_len)) {
        hit[SCAN_SHELL] = true;
    }
    if (list_matches(pat_creds, low)) hit[SCAN_CREDS] = true;
    if (list_matches(pat_keys, low))  hit[SCAN_KEYS] = true;
    if (list_matches(pat_boot, low))  hit[SCAN_BOOT] = true;

    // Prefer the most sensitive category for the "last hit" snippet.
    int order[SCAN_CATEGORY_COUNT] = {SCAN_KEYS, SCAN_CREDS, SCAN_SHELL, SCAN_BOOT};
    for (int k = 0; k < SCAN_CATEGORY_COUNT; k++) {
        int cat = order[k];
        if (hit[cat]) {
            record_hit(cat, line_buf);
            // still count the other categories that matched on this line
            for (int c = 0; c < SCAN_CATEGORY_COUNT; c++) {
                if (c != cat && hit[c]) {
                    counts[c]++;
                }
            }
            break;
        }
    }
}

void scanner_feed(char c) {
    if (c == '\n' || c == '\r') {
        scan_line();
        line_len = 0;
        return;
    }
    // Printable ASCII accumulates; other control/binary bytes act as a
    // delimiter so binary noise does not corrupt the line buffer.
    if (c >= 32 && c <= 126) {
        if (line_len >= SCAN_LINE_MAX) {
            scan_line();
            line_len = 0;
        }
        line_buf[line_len++] = c;
    } else {
        scan_line();
        line_len = 0;
    }
}

uint32_t scanner_get_count(int category) {
    if (category < 0 || category >= SCAN_CATEGORY_COUNT) {
        return 0;
    }
    return counts[category];
}

uint32_t scanner_get_total() {
    return total_hits;
}

const char* scanner_category_name(int category) {
    if (category < 0 || category >= SCAN_CATEGORY_COUNT) {
        return "?";
    }
    return category_names[category];
}

const char* scanner_last_hit() {
    return last_snip;
}

int scanner_last_category() {
    return last_cat;
}
