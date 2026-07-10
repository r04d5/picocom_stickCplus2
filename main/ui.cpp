#include "ui.h"
#include <M5Unified.h>
#include "uart_manager.h"
#include "terminal.h"
#include "autobaud.h"
#include "spammer.h"
#include "wifi_bridge.h"
#include "scanner.h"
#include "proto_analyzer.h"
#include "fuzzer.h"
#include <string.h>
#include <stdio.h>

static bool is_cardputer() {
    auto b = M5.getBoard();
    return (b == m5::board_t::board_M5Cardputer || b == m5::board_t::board_M5CardputerADV);
}

DeviceMode current_mode = MODE_TEXT_TERMINAL;

bool in_menu = true;
int menu_selection = 0;
const int MENU_ITEMS_COUNT = 7;
const char* menu_items[] = {
    "1. Text Terminal",
    "2. Hex Viewer",
    "3. Auto-Baudrate",
    "4. Spammer / Macros",
    "5. Wi-Fi Bridge",
    "6. Security Scanner",
    "7. Parser Fuzzer"
};

void draw_dashboard_static() {
    // Blue header (Header)
    M5.Display.fillRect(0, 0, 240, 18, M5.Display.color565(59, 130, 246));
    M5.Display.setTextColor(WHITE);
    M5.Display.setFont(&fonts::Font2); // Use larger 8x16 font
    M5.Display.setTextSize(1);
    
    if (in_menu) {
        M5.Display.drawString("MENU", 6, 1);
    } else {
        switch (current_mode) {
            case MODE_TEXT_TERMINAL:
                M5.Display.drawString("M5 PICOCOM", 6, 1);
                break;
            case MODE_HEX_VIEWER:
                M5.Display.drawString("HEXVIEW", 6, 1);
                break;
            case MODE_AUTO_BAUD:
                M5.Display.drawString("AUTO-B", 6, 1);
                break;
            case MODE_SPAMMER:
                M5.Display.drawString("SPAMMER", 6, 1);
                break;
            case MODE_WIFI_BRIDGE:
                M5.Display.drawString("WIFI-B", 6, 1);
                break;
            case MODE_SCANNER:
                M5.Display.drawString("SECSCAN", 6, 1);
                break;
            case MODE_FUZZER:
                M5.Display.drawString("FUZZER", 6, 1);
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

    M5.Display.setFont(&fonts::Font2); // Use larger 8x16 font for header
    M5.Display.setTextSize(1);

    char baud_str[16];
    snprintf(baud_str, sizeof(baud_str), "B:%u", (unsigned int)current_baud);
    M5.Display.setTextColor(YELLOW);
    M5.Display.drawString(baud_str, 110, 1);

    M5.Display.setTextColor(echo_mode ? GREEN : M5.Display.color565(203, 213, 225));
    M5.Display.drawString(echo_mode ? "ECHO ON" : "ECHO OFF", 180, 1);

    // Update statistics panel (RX and TX bytes)
    M5.Display.fillRect(0, 18, 240, 14, M5.Display.color565(30, 41, 59));
    M5.Display.setFont(&fonts::Font0); // Revert to standard font for stats
    M5.Display.setTextSize(1);
    
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
                M5.Display.drawString("Two detection methods:", 12, 53);
                M5.Display.setTextColor(YELLOW);
                M5.Display.drawString(is_cardputer() ? "Click G0: Pulse scan (send 'U')" : "BTN A: Pulse scan (send 'U' 0x55)", 12, 70);
                M5.Display.drawString(is_cardputer() ? "Hold  G0: Baud sweep (read log)" : "BTN B: Baud sweep (reads text/log)", 12, 85);
                M5.Display.setTextColor(M5.Display.color565(156, 163, 175));
                M5.Display.drawString(is_cardputer() ? "Dbl-click G0 to open Menu" : "Press double-B to open Menu", 12, 115);
                break;

            case AB_STATE_RUNNING:
                if (ab_method == AB_METHOD_SWEEP) {
                    M5.Display.setTextColor(YELLOW);
                    M5.Display.drawString("SWEEPING BAUD RATES...", 12, 38);
                    int idx = baud_sweep_current_idx();
                    if (idx > 7) idx = 7;
                    char sweep_buf[40];
                    snprintf(sweep_buf, sizeof(sweep_buf), "Testing: %u bps (%d/8)",
                             (unsigned int)baud_rates[idx], idx + 1);
                    M5.Display.setTextColor(WHITE);
                    M5.Display.drawString(sweep_buf, 12, 58);
                    uint32_t best = baud_sweep_best_baud();
                    char best_buf[40];
                    if (best > 0) {
                        snprintf(best_buf, sizeof(best_buf), "Best so far: %u bps", (unsigned int)best);
                    } else {
                        snprintf(best_buf, sizeof(best_buf), "Best so far: --");
                    }
                    M5.Display.setTextColor(M5.Display.color565(156, 163, 175));
                    M5.Display.drawString(best_buf, 12, 78);
                    M5.Display.setTextColor(RED);
                    M5.Display.drawString(is_cardputer() ? "Press BTN G0 to CANCEL" : "Press BTN A to CANCEL", 12, 105);
                } else {
                    M5.Display.setTextColor(YELLOW);
                    M5.Display.drawString("SCANNING UART PORT...", 12, 38);
                    M5.Display.setTextColor(WHITE);
                    M5.Display.drawString("Please send data from host...", 12, 58);
                    M5.Display.setTextColor(M5.Display.color565(156, 163, 175));
                    M5.Display.drawString("Measuring pulse timings...", 12, 78);
                    M5.Display.setTextColor(RED);
                    M5.Display.drawString(is_cardputer() ? "Press BTN G0 to CANCEL" : "Press BTN A to CANCEL", 12, 105);
                }
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
                M5.Display.drawString(is_cardputer() ? "Hold BTN G0 to APPLY" : "Press BTN B to APPLY", 12, 105);
                M5.Display.setTextColor(YELLOW);
                M5.Display.drawString(is_cardputer() ? "Press BTN G0 to RETRY" : "Press BTN A to RETRY", 12, 120);
                break;
                
            case AB_STATE_FAILED:
                M5.Display.setTextColor(RED);
                M5.Display.drawString("DETECTION FAILED!", 12, 38);
                M5.Display.setTextColor(WHITE);
                M5.Display.drawString("No clear signal or framing match.", 12, 58);
                M5.Display.drawString("Make sure host is sending 'U' (0x55).", 12, 73);
                M5.Display.setTextColor(YELLOW);
                M5.Display.drawString(is_cardputer() ? "Press BTN G0 to RETRY" : "Press BTN A to RETRY", 12, 105);
                M5.Display.setTextColor(M5.Display.color565(156, 163, 175));
                M5.Display.drawString(is_cardputer() ? "Dbl-click G0 for Menu" : "Press double-B for Menu", 12, 120);
                break;
        }
    } else if (current_mode == MODE_SPAMMER) {
        // Draw Spammer Mode UI
        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextSize(1);
        
        // List macros
        for (int i = 0; i < MACRO_COUNT; i++) {
            int y_pos = 36 + i * 11;
            if (i == selected_macro_idx) {
                M5.Display.setTextColor(YELLOW);
                M5.Display.drawString(">", 6, y_pos);
                M5.Display.drawString(macros[i].name, 16, y_pos);
            } else {
                M5.Display.setTextColor(M5.Display.color565(156, 163, 175)); // Gray
                M5.Display.drawString(macros[i].name, 16, y_pos);
            }
        }
        
        // Horizontal separator line
        M5.Display.drawFastHLine(6, 82, 228, M5.Display.color565(71, 85, 105));
        
        // Status indicator
        M5.Display.setTextColor(WHITE);
        M5.Display.drawString("Status:", 12, 88);
        if (spammer_active) {
            M5.Display.fillRect(60, 86, 64, 13, GREEN);
            M5.Display.setTextColor(BLACK);
            M5.Display.drawString(" ACTIVE ", 64, 88);
        } else {
            M5.Display.fillRect(60, 86, 64, 13, M5.Display.color565(71, 85, 105));
            M5.Display.setTextColor(WHITE);
            M5.Display.drawString("  IDLE  ", 64, 88);
        }
        
        // Packets sent counter
        char sent_buf[32];
        snprintf(sent_buf, sizeof(sent_buf), "Sent: %u pkts", (unsigned int)spam_sent_count);
        M5.Display.setTextColor(CYAN);
        M5.Display.drawString(sent_buf, 135, 88);
        
        // Help footer
        M5.Display.setTextColor(M5.Display.color565(147, 197, 253));
        M5.Display.drawString(is_cardputer() ? "G0: Cycle | Hold G0: Run | Dbl-G0: Menu" : "A: Select | B: Start/Stop | Dbl-B: Menu", 6, 108);

        // Live protocol response indicator (see Security Scanner for full stats)
        if (proto_last_event() >= 0) {
            int ev = proto_last_event();
            bool bad = (ev == PROTO_AT_ERROR || ev == PROTO_NMEA_BADCRC || ev == PROTO_MODBUS_BADCRC);
            char rd[46];
            snprintf(rd, sizeof(rd), "Resp: %s", proto_last_desc());
            M5.Display.setTextColor(bad ? RED : GREEN);
            M5.Display.drawString(rd, 6, 120);
        }
    } else if (current_mode == MODE_WIFI_BRIDGE) {
        // Draw Wi-Fi Bridge UI
        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextSize(1);
        
        M5.Display.setTextColor(M5.Display.color565(147, 197, 253)); // Soft blue
        M5.Display.drawString("WI-FI TRANSPARENT BRIDGE", 12, 38);
        
        M5.Display.setTextColor(WHITE);
        M5.Display.drawString("SSID: M5-UART-Bridge", 12, 53);
        M5.Display.drawString("TCP:  192.168.4.1:8080", 12, 68);
        
        // Client Connection Status
        M5.Display.drawString("Client:", 12, 88);
        if (client_connected) {
            M5.Display.fillRect(64, 86, 164, 13, GREEN);
            M5.Display.setTextColor(BLACK);
            char client_buf[32];
            snprintf(client_buf, sizeof(client_buf), " %s ", client_ip_str);
            M5.Display.drawString(client_buf, 68, 88);
        } else {
            M5.Display.fillRect(64, 86, 96, 13, M5.Display.color565(71, 85, 105));
            M5.Display.setTextColor(WHITE);
            M5.Display.drawString(" DISCONNECTED ", 68, 88);
        }
        
        // Help / Exit info
        M5.Display.setTextColor(M5.Display.color565(156, 163, 175)); // Gray
        M5.Display.drawString("PC run: nc 192.168.4.1 8080", 12, 108);
        M5.Display.setTextColor(YELLOW);
        M5.Display.drawString(is_cardputer() ? "Dbl-click G0 to close AP & Exit" : "Press double-B to close AP & Exit", 12, 120);
    } else if (current_mode == MODE_SCANNER) {
        // Draw Security Scanner UI (secrets/shell + protocol response analysis)
        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextSize(1);

        // ---- Security findings (package 1) ----
        M5.Display.setTextColor(M5.Display.color565(147, 197, 253)); // Soft blue
        M5.Display.drawString("SECURITY FINDINGS", 6, 34);
        char tot_buf[24];
        snprintf(tot_buf, sizeof(tot_buf), "Total: %u", (unsigned int)scanner_get_total());
        M5.Display.setTextColor(scanner_get_total() > 0 ? RED : M5.Display.color565(156, 163, 175));
        M5.Display.drawString(tot_buf, 160, 34);

        char l1[40];
        snprintf(l1, sizeof(l1), "SHELL:%-4u  CREDS:%u",
                 (unsigned int)scanner_get_count(SCAN_SHELL),
                 (unsigned int)scanner_get_count(SCAN_CREDS));
        M5.Display.setTextColor(WHITE);
        M5.Display.drawString(l1, 6, 46);
        char l2[40];
        snprintf(l2, sizeof(l2), "KEYS :%-4u  BOOT :%u",
                 (unsigned int)scanner_get_count(SCAN_KEYS),
                 (unsigned int)scanner_get_count(SCAN_BOOT));
        M5.Display.drawString(l2, 6, 56);

        // Last matched line, tagged with its category
        int lc = scanner_last_category();
        if (lc >= 0) {
            char hit[46];
            snprintf(hit, sizeof(hit), "[%s] %s", scanner_category_name(lc), scanner_last_hit());
            M5.Display.setTextColor(M5.Display.color565(251, 191, 36)); // Amber
            M5.Display.drawString(hit, 6, 68);
        } else {
            M5.Display.setTextColor(M5.Display.color565(100, 116, 139));
            M5.Display.drawString("(listening for exposures...)", 6, 68);
        }

        M5.Display.drawFastHLine(6, 80, 228, M5.Display.color565(71, 85, 105));

        // ---- Protocol response analysis (package 3) ----
        M5.Display.setTextColor(M5.Display.color565(147, 197, 253));
        M5.Display.drawString("PROTOCOL ANALYSIS", 6, 84);

        char p1[40];
        snprintf(p1, sizeof(p1), "AT  ok:%-3u err:%u",
                 (unsigned int)proto_count(PROTO_AT_OK),
                 (unsigned int)proto_count(PROTO_AT_ERROR));
        M5.Display.setTextColor(WHITE);
        M5.Display.drawString(p1, 6, 96);
        char p2[40];
        snprintf(p2, sizeof(p2), "NMEA ok:%-3u bad:%u   MB ok:%-3u bad:%u",
                 (unsigned int)proto_count(PROTO_NMEA_OK),
                 (unsigned int)proto_count(PROTO_NMEA_BADCRC),
                 (unsigned int)proto_count(PROTO_MODBUS_OK),
                 (unsigned int)proto_count(PROTO_MODBUS_BADCRC));
        M5.Display.drawString(p2, 6, 106);

        if (proto_last_event() >= 0) {
            int ev = proto_last_event();
            bool bad = (ev == PROTO_AT_ERROR || ev == PROTO_NMEA_BADCRC || ev == PROTO_MODBUS_BADCRC);
            char pd[46];
            snprintf(pd, sizeof(pd), "Last: %s", proto_last_desc());
            M5.Display.setTextColor(bad ? RED : GREEN);
            M5.Display.drawString(pd, 6, 118);
        }
    } else if (current_mode == MODE_FUZZER) {
        // Draw Parser Fuzzer UI
        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextSize(1);

        // List fuzz cases with a selector
        for (int i = 0; i < FUZZ_CASE_COUNT; i++) {
            int y_pos = 35 + i * 10;
            if (i == fuzz_case_idx) {
                M5.Display.setTextColor(YELLOW);
                M5.Display.drawString(">", 6, y_pos);
                M5.Display.drawString(fuzz_case_name(i), 16, y_pos);
            } else {
                M5.Display.setTextColor(M5.Display.color565(156, 163, 175));
                M5.Display.drawString(fuzz_case_name(i), 16, y_pos);
            }
        }

        M5.Display.drawFastHLine(6, 96, 228, M5.Display.color565(71, 85, 105));

        // Status + sent counter
        M5.Display.setTextColor(WHITE);
        M5.Display.drawString(fuzzer_active ? "FUZZING" : "IDLE", 6, 100);
        char fsent[24];
        snprintf(fsent, sizeof(fsent), "Sent: %u", (unsigned int)fuzz_sent_count);
        M5.Display.setTextColor(CYAN);
        M5.Display.drawString(fsent, 70, 100);

        // Target health indicator -- the actual finding
        int h = fuzzer_health();
        const char* htxt;
        uint16_t hcol;
        switch (h) {
            case FZ_RESPONSIVE: htxt = "TARGET: RESPONSIVE"; hcol = GREEN; break;
            case FZ_SILENT:     htxt = "TARGET: SILENT...";  hcol = YELLOW; break;
            case FZ_HANG:       htxt = "TARGET: HANG/DoS!";  hcol = RED; break;
            default:            htxt = "TARGET: no reply yet"; hcol = M5.Display.color565(156, 163, 175); break;
        }
        M5.Display.setTextColor(hcol);
        M5.Display.drawString(htxt, 6, 112);

        M5.Display.setTextColor(M5.Display.color565(147, 197, 253));
        M5.Display.drawString(is_cardputer() ? "G0:Case Hold:Run Dbl:Menu" : "A:Case B:Run Dbl-B:Menu", 6, 124);
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.drawString("Mode not implemented yet.", 12, 45);
        M5.Display.drawString(is_cardputer() ? "Dbl-click G0 to return" : "Press double-B to return", 12, 60);
        M5.Display.drawString("to Menu.", 12, 75);
    }
}

void draw_menu() {
    M5.Display.fillRect(1, 33, 238, 101, M5.Display.color565(15, 23, 42)); // Dark background
    M5.Display.setFont(&fonts::Font2); // Use larger 8x16 font
    M5.Display.setTextSize(1);
    
    for (int i = 0; i < MENU_ITEMS_COUNT; i++) {
        int y_pos = 34 + i * 14; // Spacing of 14 pixels to fit all items
        if (i == menu_selection) {
            // Selected item: Indigo background, white text
            M5.Display.fillRect(6, y_pos - 1, 228, 13, M5.Display.color565(79, 70, 229));
            M5.Display.setTextColor(WHITE);
        } else {
            // Non-selected item: light gray text
            M5.Display.setTextColor(M5.Display.color565(156, 163, 175));
        }
        M5.Display.drawString(menu_items[i], 12, y_pos);
    }
}
