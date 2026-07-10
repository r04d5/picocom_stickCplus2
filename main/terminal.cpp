#include "terminal.h"
#include <string.h>
#include <stdio.h>

// Terminal Buffer
char terminal_buf[MAX_ROWS][MAX_COLS + 1];
int current_row = 0;
int current_col = 0;

// Hex Viewer Buffer
char hex_rows[MAX_ROWS][45];
int current_hex_row = 0;
uint32_t hex_byte_count = 0;
uint8_t current_line_bytes[8];
int current_line_len = 0;

void add_char_to_terminal(char c) {
    if (c == '\r') {
        current_col = 0;
        return;
    }
    if (c == '\n') {
        current_row++;
        if (current_row >= MAX_ROWS) {
            // Scroll lines up
            for (int i = 1; i < MAX_ROWS; i++) {
                strcpy(terminal_buf[i - 1], terminal_buf[i]);
            }
            current_row = MAX_ROWS - 1;
        }
        current_col = 0;
        memset(terminal_buf[current_row], 0, sizeof(terminal_buf[current_row]));
        return;
    }

    // Only display standard printable characters
    if (c >= 32 && c <= 126) {
        if (current_col >= MAX_COLS - 1) {
            add_char_to_terminal('\n');
        }
        terminal_buf[current_row][current_col++] = c;
        terminal_buf[current_row][current_col] = '\0';
    }
}

void format_hex_line(int row, uint32_t addr, uint8_t *bytes, int len) {
    char *buf = hex_rows[row];
    int written = snprintf(buf, 45, "%03X: ", (unsigned int)(addr & 0xFFF));
    for (int i = 0; i < 8; i++) {
        if (i < len) {
            written += snprintf(buf + written, 45 - written, "%02X ", bytes[i]);
        } else {
            written += snprintf(buf + written, 45 - written, "   ");
        }
    }
    // Overwrite the last space before the pipe for compactness
    if (buf[written - 1] == ' ') {
        buf[written - 1] = '|';
    } else {
        buf[written++] = '|';
    }
    for (int i = 0; i < 8; i++) {
        if (i < len) {
            char c = bytes[i];
            if (c >= 32 && c <= 126) {
                buf[written++] = c;
            } else {
                buf[written++] = '.';
            }
        } else {
            buf[written++] = ' ';
        }
    }
    buf[written] = '\0';
}

void add_byte_to_hex_terminal(uint8_t b) {
    current_line_bytes[current_line_len++] = b;
    hex_byte_count++;
    
    uint32_t line_addr = hex_byte_count - current_line_len;
    format_hex_line(current_hex_row, line_addr, current_line_bytes, current_line_len);
    
    if (current_line_len == 8) {
        current_line_len = 0;
        current_hex_row++;
        if (current_hex_row >= MAX_ROWS) {
            for (int i = 1; i < MAX_ROWS; i++) {
                strcpy(hex_rows[i - 1], hex_rows[i]);
            }
            current_hex_row = MAX_ROWS - 1;
        }
        memset(hex_rows[current_hex_row], 0, sizeof(hex_rows[current_hex_row]));
    }
}

void clear_terminal_buffers() {
    for (int i = 0; i < MAX_ROWS; i++) {
        memset(terminal_buf[i], 0, sizeof(terminal_buf[i]));
        memset(hex_rows[i], 0, sizeof(hex_rows[i]));
    }
    current_row = 0;
    current_col = 0;
    current_hex_row = 0;
    hex_byte_count = 0;
    current_line_len = 0;
}
