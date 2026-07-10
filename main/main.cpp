#include <M5Unified.h>
#include "uart_manager.h"
#include "terminal.h"
#include "autobaud.h"
#include "spammer.h"
#include "wifi_bridge.h"
#include "scanner.h"
#include "proto_analyzer.h"
#include "fuzzer.h"
#include "line_monitor.h"
#include "tester.h"
#include "keyboard.h"
#include "ui.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/uart_ll.h>

extern "C" void app_main() {
    // Initialize M5Unified
    auto cfg = M5.config();
    M5.begin(cfg);

    // Configure display in landscape mode
    M5.Display.setRotation(1);

    // Initialize the terminal buffers with empty strings
    clear_terminal_buffers();
    line_monitor_reset();

    // Bring up the Cardputer matrix keyboard (no-op on other boards).
    keyboard_init();

    // Initialize screen with basic layout
    draw_dashboard_static();
    draw_dashboard_dynamic();
    if (in_menu) {
        draw_menu();
    } else {
        draw_terminal();
    }

    // Initialize UART1 with standard baud rate (115200)
    init_uart(current_baud);

    // Create Queue for UART input buffering (1024 character capacity)
    rx_queue = xQueueCreate(1024, sizeof(char));

    // Start UART RX Task on core 0
    xTaskCreatePinnedToCore(uart_rx_task, "uart_rx_task", 4096, NULL, 5, NULL, 0);

    bool terminal_dirty = false;
    uint32_t last_draw_time = 0;

    bool prev_line_stuck = false;

    while (1) {
        // Update button readings
        M5.update();

        // Line-anomaly monitor: sample the RX pin and refresh the status bar
        // indicator when a sustained stuck-low appears or clears.
        line_monitor_update(xTaskGetTickCount() * portTICK_PERIOD_MS);
        if (line_stuck_low() != prev_line_stuck) {
            prev_line_stuck = line_stuck_low();
            draw_dashboard_dynamic();
        }

        // Polling check for Auto-Baudrate detection completion or timeout
        if (!in_menu && current_mode == MODE_AUTO_BAUD && ab_state == AB_STATE_RUNNING) {
            if (ab_method == AB_METHOD_SWEEP) {
                int prev_idx = baud_sweep_current_idx();
                baud_sweep_step();
                // Redraw on each rate advance (progress) and on completion.
                if (ab_state != AB_STATE_RUNNING || baud_sweep_current_idx() != prev_idx) {
                    draw_terminal();
                    draw_dashboard_static();
                    draw_dashboard_dynamic();
                }
            } else {
                uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
                uart_dev_t *hw = UART_LL_GET_HW(UART_NUM_1);
                uint32_t edges = uart_ll_get_rxd_edge_cnt(hw);

                if (edges >= 40 || (now_ms - ab_start_time >= 2000)) {
                    stop_autobaud_detection();
                    draw_terminal();
                    draw_dashboard_static();
                    draw_dashboard_dynamic();
                }
            }
        }

        // Determine if we are on Cardputer to adapt button gestures
        bool is_cardputer = (M5.getBoard() == m5::board_t::board_M5Cardputer || M5.getBoard() == m5::board_t::board_M5CardputerADV);
        
        bool toggle_menu_pressed = false;
        bool select_next_pressed = false;
        bool confirm_action_pressed = false;
        bool test_packet_pressed = false;
        
        if (is_cardputer) {
            toggle_menu_pressed = M5.BtnA.wasDoubleClicked();
            select_next_pressed = M5.BtnA.wasClicked();
            confirm_action_pressed = M5.BtnA.wasHold();
        } else {
            toggle_menu_pressed = M5.BtnB.wasDoubleClicked() || M5.BtnPWR.wasClicked();
            select_next_pressed = M5.BtnA.wasClicked();
            confirm_action_pressed = M5.BtnB.wasSingleClicked();
            test_packet_pressed = M5.BtnB.wasHold();
        }

        // --- Cardputer keyboard input -------------------------------------
        // Poll once per loop; events are edge-triggered (newly pressed keys).
        kb_event_t kb_events[16];
        int kb_n = keyboard_poll(kb_events, 16);
        bool kb_enter_any = false;
        for (int k = 0; k < kb_n; k++) {
            if (kb_events[k].type == KB_ESC) toggle_menu_pressed = true; // ESC = Fn+`
            else if (kb_events[k].type == KB_ENTER) kb_enter_any = true;
        }
        // In terminal/hex the keyboard types onto the wire, so Enter must NOT
        // double as the confirm/action button there. Everywhere else it does.
        bool typing_mode = (current_mode == MODE_TEXT_TERMINAL || current_mode == MODE_HEX_VIEWER);
        if (in_menu || !typing_mode) {
            confirm_action_pressed = confirm_action_pressed || kb_enter_any;
        }

        // Detect open/close menu trigger
        if (toggle_menu_pressed) {
            if (ab_state == AB_STATE_RUNNING) {
                if (ab_method == AB_METHOD_SWEEP) {
                    cancel_baud_sweep();
                } else {
                    stop_autobaud_detection();
                }
                ab_state = AB_STATE_IDLE;
            }
            if (current_mode == MODE_WIFI_BRIDGE) {
                stop_wifi_bridge();
            }
            spammer_active = false; // Disable active macro transmission
            in_menu = !in_menu;
            draw_dashboard_static();
            draw_dashboard_dynamic();
            if (in_menu) {
                draw_menu();
            } else {
                draw_terminal();
            }
        }

        if (in_menu) {
            // Keyboard directional navigation over the 2-column grid. Arrow
            // keys (Fn layer) and the bare arrow-legend keys ; . , / both work.
            for (int k = 0; k < kb_n; k++) {
                kb_event_t e = kb_events[k];
                int delta = 0;
                if (e.type == KB_UP    || (e.type == KB_CHAR && e.ch == ';')) delta = -2;
                else if (e.type == KB_DOWN  || (e.type == KB_CHAR && e.ch == '.')) delta = +2;
                else if (e.type == KB_LEFT  || (e.type == KB_CHAR && e.ch == ',')) delta = -1;
                else if (e.type == KB_RIGHT || (e.type == KB_CHAR && e.ch == '/')) delta = +1;
                if (delta != 0) {
                    menu_selection = ((menu_selection + delta) % MENU_ITEMS_COUNT + MENU_ITEMS_COUNT) % MENU_ITEMS_COUNT;
                    draw_menu();
                }
            }

            // Menu Navigation
            if (select_next_pressed) {
                menu_selection = (menu_selection + 1) % MENU_ITEMS_COUNT;
                draw_menu();
            } else if (confirm_action_pressed) {
                current_mode = (DeviceMode)menu_selection;
                in_menu = false;
                
                if (current_mode == MODE_AUTO_BAUD) {
                    ab_state = AB_STATE_IDLE;
                }
                if (current_mode == MODE_SPAMMER) {
                    spammer_active = false;
                    spam_sent_count = 0;
                }
                if (current_mode == MODE_WIFI_BRIDGE) {
                    start_wifi_bridge();
                }
                if (current_mode == MODE_FUZZER) {
                    fuzzer_reset();
                }
                if (current_mode == MODE_TESTER) {
                    tester_reset();
                }

                draw_dashboard_static();
                draw_dashboard_dynamic();
                draw_terminal();
            }
        } else {
            // Cardputer keyboard: in the terminal/hex screens, typed keys are
            // written straight onto the UART line so the user can drive a
            // remote shell. Characters echoed back by the target appear via RX.
            if (typing_mode && kb_n > 0) {
                bool sent = false;
                for (int k = 0; k < kb_n; k++) {
                    char b;
                    switch (kb_events[k].type) {
                        case KB_CHAR:      b = kb_events[k].ch; break;
                        case KB_ENTER:     b = '\r';           break;
                        case KB_BACKSPACE: b = 0x08;           break;
                        case KB_TAB:       b = '\t';           break;
                        default:           continue;
                    }
                    uart_write_bytes(UART_NUM_1, &b, 1);
                    tx_bytes++;
                    sent = true;
                }
                if (sent) draw_dashboard_dynamic();
            }

            // Normal controls in active modes
            // Front Button (BtnA)
            if (select_next_pressed) {
                if (current_mode == MODE_AUTO_BAUD) {
                    if (ab_state == AB_STATE_IDLE || ab_state == AB_STATE_SUCCESS || ab_state == AB_STATE_FAILED) {
                        start_autobaud_detection();
                        draw_terminal();
                        draw_dashboard_static();
                        draw_dashboard_dynamic();
                    } else if (ab_state == AB_STATE_RUNNING) {
                        // Cancel (whichever method is running)
                        if (ab_method == AB_METHOD_SWEEP) {
                            cancel_baud_sweep();
                        } else {
                            stop_autobaud_detection();
                        }
                        ab_state = AB_STATE_IDLE;
                        draw_terminal();
                        draw_dashboard_static();
                        draw_dashboard_dynamic();
                    }
                } else if (current_mode == MODE_SPAMMER) {
                    selected_macro_idx = (selected_macro_idx + 1) % MACRO_COUNT;
                    terminal_dirty = true;
                } else if (current_mode == MODE_FUZZER) {
                    fuzz_case_idx = (fuzz_case_idx + 1) % FUZZ_CASE_COUNT;
                    terminal_dirty = true;
                } else if (current_mode == MODE_TESTER) {
                    tester_reset();
                    terminal_dirty = true;
                } else {
                    baud_idx = (baud_idx + 1) % 8; // We have 8 standard rates
                    current_baud = baud_rates[baud_idx];
                    init_uart(current_baud);
                    draw_dashboard_dynamic();
                }
            } else if (!is_cardputer && M5.BtnA.wasHold()) {
                if (current_mode != MODE_AUTO_BAUD && current_mode != MODE_SPAMMER && current_mode != MODE_FUZZER && current_mode != MODE_TESTER) {
                    // Press and hold (BtnA on StickC only): Full reset of active screen and counters
                    clear_terminal_buffers();
                    scanner_reset();
                    proto_reset();
                    rx_bytes = 0;
                    tx_bytes = 0;
                    draw_dashboard_dynamic();
                    draw_terminal();
                }
            }

            // Confirm / Action button trigger
            if (confirm_action_pressed) {
                if (current_mode == MODE_AUTO_BAUD) {
                    if (ab_state == AB_STATE_SUCCESS) {
                        // Apply the detected baudrate
                        current_baud = ab_detected_baud;
                        // Find index of current_baud in baud_rates
                        for (int i = 0; i < 8; i++) {
                            if (baud_rates[i] == current_baud) {
                                baud_idx = i;
                                break;
                            }
                        }
                        init_uart(current_baud);
                        current_mode = MODE_TEXT_TERMINAL;
                        ab_state = AB_STATE_IDLE;

                        draw_dashboard_static();
                        draw_dashboard_dynamic();
                        draw_terminal();
                    } else if (ab_state == AB_STATE_IDLE || ab_state == AB_STATE_FAILED) {
                        // Start the passive brute-force baud sweep.
                        start_baud_sweep();
                        draw_terminal();
                        draw_dashboard_static();
                        draw_dashboard_dynamic();
                    }
                } else if (current_mode == MODE_SPAMMER) {
                    spammer_active = !spammer_active;
                    terminal_dirty = true;
                } else if (current_mode == MODE_FUZZER) {
                    if (!fuzzer_active) {
                        fuzzer_start();
                    } else {
                        fuzzer_stop();
                    }
                    terminal_dirty = true;
                } else if (current_mode == MODE_TESTER) {
                    if (!tester_active) {
                        tester_start();
                    } else {
                        tester_stop();
                    }
                    terminal_dirty = true;
                } else {
                    echo_mode = !echo_mode;
                    draw_dashboard_dynamic();
                }
            } else if (test_packet_pressed) {
                if (current_mode != MODE_AUTO_BAUD && current_mode != MODE_SPAMMER && current_mode != MODE_FUZZER && current_mode != MODE_TESTER) {
                    // Press and hold (BtnB on StickC only): Sends a test packet over serial
                    const char *test_msg = "\r\n[StickC-Plus2 UART Test Packet]\r\n";
                    int len = strlen(test_msg);
                    uart_write_bytes(UART_NUM_1, test_msg, len);
                    tx_bytes += len;
                    draw_dashboard_dynamic();
                }
            }
        }

        // Execute Spammer tick logic
        if (!in_menu && current_mode == MODE_SPAMMER) {
            uint32_t prev_sent = spam_sent_count;
            run_spammer_tick();
            if (spam_sent_count != prev_sent) {
                terminal_dirty = true;
            }
        }

        // Execute Fuzzer tick logic and refresh on health/counter changes
        if (!in_menu && current_mode == MODE_FUZZER) {
            static int prev_health = -1;
            uint32_t prev_sent = fuzz_sent_count;
            run_fuzzer_tick();
            int h = fuzzer_health();
            if (fuzz_sent_count != prev_sent || h != prev_health) {
                terminal_dirty = true;
                prev_health = h;
            }
        }

        // Execute self-test steps and refresh as each step resolves
        if (!in_menu && current_mode == MODE_TESTER && tester_active) {
            int prev_step = tester_current_step();
            tester_step();
            if (tester_current_step() != prev_step || !tester_active) {
                terminal_dirty = true;
            }
        }

        // Check Wi-Fi bridge connection status changes
        if (!in_menu && current_mode == MODE_WIFI_BRIDGE) {
            static bool prev_connected = false;
            if (client_connected != prev_connected) {
                terminal_dirty = true;
                prev_connected = client_connected;
            }
        }

        // Process all characters that arrived in the Queue if not in the Menu
        char c;
        bool data_received = false;
        while (xQueueReceive(rx_queue, &c, 0) == pdTRUE) {
            rx_bytes++;
            data_received = true;

            // Passive security scan and protocol analysis run on every byte,
            // regardless of the active mode.
            scanner_feed(c);
            proto_feed((uint8_t)c);

            if (current_mode == MODE_TEXT_TERMINAL) {
                add_char_to_terminal(c);
            } else if (current_mode == MODE_HEX_VIEWER) {
                add_byte_to_hex_terminal((uint8_t)c);
            }

            // Echo back on UART1 if enabled
            if (echo_mode) {
                uart_write_bytes(UART_NUM_1, &c, 1);
                tx_bytes++;
            }
        }

        if (data_received) {
            // The drained burst marks an inter-frame idle boundary for the
            // binary (Modbus) analyzer.
            proto_frame_boundary();
            // Note RX liveness for the fuzzer's target-health detection.
            fuzzer_note_rx(xTaskGetTickCount() * portTICK_PERIOD_MS);
            if (!in_menu) {
                terminal_dirty = true;
            }
        }

        // Refresh rate limit for screen updates (FPS) - Prevents buffer lag and flickering
        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (terminal_dirty && (now_ms - last_draw_time >= 50)) {
            draw_terminal();
            draw_dashboard_dynamic();
            terminal_dirty = false;
            last_draw_time = now_ms;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
