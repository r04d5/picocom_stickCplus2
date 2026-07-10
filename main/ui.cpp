#include "ui.h"
#include <M5Unified.h>
#include "uart_manager.h"
#include "terminal.h"
#include "autobaud.h"
#include <string.h>
#include <stdio.h>

DeviceMode current_mode = MODE_TEXT_TERMINAL;

bool in_menu = false;
int menu_selection = 0;
const int MENU_ITEMS_COUNT = 5;
const char* menu_items[] = {
    "1. Text Terminal",
    "2. Hex Viewer",
    "3. Auto-Baudrate",
    "4. Spammer / Macros",
    "5. Wi-Fi Bridge"
};

void draw_dashboard_static() {
    // Blue header (Header)
    M5.Display.fillRect(0, 0, 240, 18, M5.Display.color565(59, 130, 246));
    M5.Display.setTextColor(WHITE);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);
    
    if (in_menu) {
        M5.Display.drawString("STCP2 MENU", 6, 5);
    } else {
        switch (current_mode) {
            case MODE_TEXT_TERMINAL:
                M5.Display.drawString("STCP2 PICOCOM", 6, 5);
                break;
            case MODE_HEX_VIEWER:
                M5.Display.drawString("STCP2 HEXVIEW", 6, 5);
                break;
            case MODE_AUTO_BAUD:
                M5.Display.drawString("STCP2 AUTO-B", 6, 5);
                break;
            case MODE_SPAMMER:
                M5.Display.drawString("STCP2 SPAMMER", 6, 5);
                break;
            case MODE_WIFI_BRIDGE:
                M5.Display.drawString("STCP2 WIFI-B", 6, 5);
                break;
        }
    }

    // Statistics Panel (Dark gray)
    M5.Display.fillRect(0, 18, 240, 14, M5.Display.color565(30, 41, 59));

    // Terminal Window Background (Dark black) and borders
    M5.Display.fillRect(0, 32, 240, 103, M5.Display.color565(15, 23, 42));
    if (in_menu) {
        M5.Display.drawRect(0, 32, 240, 103, M5.Display.color565(99, 102, 241)); // Indigo border
    } else {
        M5.Display.drawRect(0, 32, 240, 103, M5.Display.color565(51, 65, 85)); // Slate border
    }
}

void draw_dashboard_dynamic() {
    // Update right side of Header (Baud Rate and Echo status)
    M5.Display.fillRect(100, 0, 140, 18, M5.Display.color565(59, 130, 246));

    char baud_str[16];
    snprintf(baud_str, sizeof(baud_str), "B:%u", (unsigned int)current_baud);
    M5.Display.setTextColor(YELLOW);
    M5.Display.drawString(baud_str, 110, 5);

    M5.Display.setTextColor(echo_mode ? GREEN : M5.Display.color565(203, 213, 225));
    M5.Display.drawString(echo_mode ? "ECHO ON" : "ECHO OFF", 180, 5);

    // Update statistics panel (RX and TX bytes)
    M5.Display.fillRect(0, 18, 240, 14, M5.Display.color565(30, 41, 59));
    
    char rx_str[32];
    snprintf(rx_str, sizeof(rx_str), "RX: %u B", (unsigned int)rx_bytes);
    M5.Display.setTextColor(CYAN);
    M5.Display.drawString(rx_str, 6, 21);

    char tx_str[32];
    snprintf(tx_str, sizeof(tx_str), "TX: %u B", (unsigned int)tx_bytes);
    M5.Display.setTextColor(ORANGE);
    M5.Display.drawString(tx_str, 120, 21);
}

void draw_terminal() {
    // Clear inner terminal area
    M5.Display.fillRect(1, 33, 238, 101, M5.Display.color565(15, 23, 42));
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);

    if (current_mode == MODE_TEXT_TERMINAL) {
        M5.Display.setTextColor(M5.Display.color565(74, 222, 128)); // Terminal Green
        for (int i = 0; i < MAX_ROWS; i++) {
            if (strlen(terminal_buf[i]) > 0 || i <= current_row) {
                M5.Display.drawString(terminal_buf[i], 6, 35 + i * 9);
            }
        }
    } else if (current_mode == MODE_HEX_VIEWER) {
        M5.Display.setTextColor(M5.Display.color565(251, 191, 36)); // Amber/Yellow
        for (int i = 0; i < MAX_ROWS; i++) {
            if (strlen(hex_rows[i]) > 0 || i <= current_hex_row) {
                M5.Display.drawString(hex_rows[i], 6, 35 + i * 9);
            }
        }
    } else if (current_mode == MODE_AUTO_BAUD) {
        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextSize(1);
        
        switch (ab_state) {
            case AB_STATE_IDLE:
                M5.Display.setTextColor(M5.Display.color565(147, 197, 253)); // Soft blue
                M5.Display.drawString("AUTO-BAUDRATE DETECTION", 12, 38);
                M5.Display.setTextColor(WHITE);
                M5.Display.drawString("1. Connect target RX/TX to Grove.", 12, 53);
                M5.Display.drawString("2. Send repeating chars (e.g. 'U').", 12, 68);
                M5.Display.setTextColor(YELLOW);
                M5.Display.drawString("Press BTN A to START SCAN", 12, 88);
                M5.Display.setTextColor(M5.Display.color565(156, 163, 175));
                M5.Display.drawString("Press double-B to open Menu", 12, 115);
                break;
                
            case AB_STATE_RUNNING:
                M5.Display.setTextColor(YELLOW);
                M5.Display.drawString("SCANNING UART PORT...", 12, 38);
                M5.Display.setTextColor(WHITE);
                M5.Display.drawString("Please send data from host...", 12, 58);
                M5.Display.setTextColor(M5.Display.color565(156, 163, 175));
                M5.Display.drawString("Measuring pulse timings...", 12, 78);
                M5.Display.setTextColor(RED);
                M5.Display.drawString("Press BTN A to CANCEL", 12, 105);
                break;
                
            case AB_STATE_SUCCESS:
                M5.Display.setTextColor(GREEN);
                M5.Display.drawString("DETECTION SUCCESSFUL!", 12, 38);
                M5.Display.setTextColor(WHITE);
                M5.Display.drawString("Detected Baud Rate:", 12, 58);
                M5.Display.setFont(&fonts::Font2); // Slightly larger font
                M5.Display.setTextColor(YELLOW);
                char baud_buf[32];
                snprintf(baud_buf, sizeof(baud_buf), "%u bps", (unsigned int)ab_detected_baud);
                M5.Display.drawString(baud_buf, 24, 75);
                
                M5.Display.setFont(&fonts::Font0);
                M5.Display.setTextColor(GREEN);
                M5.Display.drawString("Press BTN B to APPLY", 12, 105);
                M5.Display.setTextColor(YELLOW);
                M5.Display.drawString("Press BTN A to RETRY", 12, 120);
                break;
                
            case AB_STATE_FAILED:
                M5.Display.setTextColor(RED);
                M5.Display.drawString("DETECTION FAILED!", 12, 38);
                M5.Display.setTextColor(WHITE);
                M5.Display.drawString("No clear signal or framing match.", 12, 58);
                M5.Display.drawString("Make sure host is sending 'U' (0x55).", 12, 73);
                M5.Display.setTextColor(YELLOW);
                M5.Display.drawString("Press BTN A to RETRY", 12, 105);
                M5.Display.setTextColor(M5.Display.color565(156, 163, 175));
                M5.Display.drawString("Press double-B for Menu", 12, 120);
                break;
        }
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.drawString("Mode not implemented yet.", 12, 45);
        M5.Display.drawString("Press double-B to return", 12, 60);
        M5.Display.drawString("to Menu.", 12, 75);
    }
}

void draw_menu() {
    M5.Display.fillRect(1, 33, 238, 101, M5.Display.color565(15, 23, 42)); // Dark background
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);
    
    for (int i = 0; i < MENU_ITEMS_COUNT; i++) {
        int y_pos = 36 + i * 16;
        if (i == menu_selection) {
            // Selected item: Indigo background, white text
            M5.Display.fillRect(6, y_pos - 2, 228, 14, M5.Display.color565(79, 70, 229));
            M5.Display.setTextColor(WHITE);
        } else {
            // Non-selected item: light gray text
            M5.Display.setTextColor(M5.Display.color565(156, 163, 175));
        }
        M5.Display.drawString(menu_items[i], 12, y_pos);
    }
}
