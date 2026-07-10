#pragma once

#include <stdint.h>

#define MAX_COLS 39
#define MAX_ROWS 11

// Terminal Buffer
extern char terminal_buf[MAX_ROWS][MAX_COLS + 1];
extern int current_row;
extern int current_col;

// Hex Viewer Buffer
extern char hex_rows[MAX_ROWS][45];
extern int current_hex_row;
extern uint32_t hex_byte_count;
extern uint8_t current_line_bytes[8];
extern int current_line_len;

// Function prototypes
void add_char_to_terminal(char c);
void add_byte_to_hex_terminal(uint8_t b);
void format_hex_line(int row, uint32_t addr, uint8_t *bytes, int len);
void clear_terminal_buffers();
