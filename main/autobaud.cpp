#include "autobaud.h"
#include "uart_manager.h"
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/rtc.h>
#include <esp_clk_tree.h>
#include <hal/uart_ll.h>
#include <stdio.h>

AutoBaudState ab_state = AB_STATE_IDLE;
uint32_t ab_detected_baud = 0;
uint32_t ab_start_time = 0;

uint32_t snap_to_standard_baud(uint32_t calculated) {
    uint32_t best_baud = 0;
    uint32_t min_diff = 0xFFFFFFFF;
    
    for (int i = 0; i < 8; i++) {
        uint32_t b = baud_rates[i];
        uint32_t diff = (calculated > b) ? (calculated - b) : (b - calculated);
        if (diff < min_diff) {
            min_diff = diff;
            best_baud = b;
        }
    }
    
    if (best_baud > 0) {
        uint32_t tolerance = best_baud * 15 / 100;
        if (min_diff <= tolerance) {
            return best_baud;
        }
    }
    return 0;
}

#if CONFIG_IDF_TARGET_ESP32
#define AUTOBAUD_CLK_SRC UART_SCLK_REF_TICK
#else
#define AUTOBAUD_CLK_SRC UART_SCLK_RTC
#endif

uint32_t get_rtc_fast_freq() {
#if CONFIG_IDF_TARGET_ESP32
    return 1000000; // REF_TICK on ESP32 is exactly 1 MHz
#else
    uint32_t clock_hz = 17500000; // standard fallback
    if (esp_clk_tree_src_get_freq_hz((soc_module_clk_t)UART_SCLK_RTC, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &clock_hz) == ESP_OK) {
        return clock_hz;
    }
    return clock_hz;
#endif
}

void start_autobaud_detection() {
    // 1. Temporarily pause rx task reading
    uart_rx_enabled = false;
    vTaskDelay(pdMS_TO_TICKS(20));
    
    // 2. Re-configure UART1 with target-specific clock source
    if (uart_is_driver_installed(UART_NUM_1)) {
        uart_driver_delete(UART_NUM_1);
    }
    
    uart_config_t uart_config = {};
    uart_config.baud_rate = 115200; // Dummy
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = AUTOBAUD_CLK_SRC; // Set target-specific clock source
    uart_config.rx_flow_ctrl_thresh = 122;
    
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, get_grove_tx_pin(), get_grove_rx_pin(), UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // Install driver again
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024, 1024, 0, NULL, 0));
    
    uart_dev_t *hw = UART_LL_GET_HW(UART_NUM_1);
    
    // 3. Clear UART interrupts and reset the auto-baud status
    uart_ll_disable_intr_mask(hw, ~0);
    uart_ll_clr_intsts_mask(hw, ~0);
    
    // 4. Enable auto-baudrate detection hardware
    uart_ll_set_autobaud_en(hw, true);
    
    // 5. Update state
    ab_state = AB_STATE_RUNNING;
    ab_detected_baud = 0;
    ab_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    printf("[AUTOBAUD] Started detection using configured clock source\n");
}

void stop_autobaud_detection() {
    uart_dev_t *hw = UART_LL_GET_HW(UART_NUM_1);
    
    // Disable auto-baudrate detection
    uart_ll_set_autobaud_en(hw, false);
    
    // Read the measured values
    uint32_t edge_cnt = uart_ll_get_rxd_edge_cnt(hw);
    uint32_t low_period = uart_ll_get_low_pulse_cnt(hw);
    uint32_t high_period = uart_ll_get_high_pulse_cnt(hw);
    uint32_t pos_period = uart_ll_get_pos_pulse_cnt(hw);
    uint32_t neg_period = uart_ll_get_neg_pulse_cnt(hw);
    
    printf("[AUTOBAUD] Stopped detection. Edges: %u, Low: %u, High: %u, Pos: %u, Neg: %u\n",
           (unsigned int)edge_cnt, (unsigned int)low_period, (unsigned int)high_period,
           (unsigned int)pos_period, (unsigned int)neg_period);
    
    // Deinit driver and revert to standard config
    if (uart_is_driver_installed(UART_NUM_1)) {
        uart_driver_delete(UART_NUM_1);
    }
    
    // Re-initialize with standard settings (using APB clock, current baud rate)
    init_uart(current_baud);
    uart_rx_enabled = true;
    
    // Verify edge count
    if (edge_cnt < 8) {
        printf("[AUTOBAUD] Failed: Insufficient edges detected (%u)\n", (unsigned int)edge_cnt);
        ab_state = AB_STATE_FAILED;
        return;
    }
    
    // Verify pulse count values to avoid division by zero or overflow
    if (low_period == 0 || low_period >= 4095 || high_period == 0 || high_period >= 4095) {
        printf("[AUTOBAUD] Failed: Pulse values out of bounds (Low: %u, High: %u)\n", 
               (unsigned int)low_period, (unsigned int)high_period);
        ab_state = AB_STATE_FAILED;
        return;
    }
    
    // Get RTC fast clock frequency and calculate baud rate
    uint32_t rtc_freq = get_rtc_fast_freq();
    uint32_t calculated_baud = (rtc_freq * 2) / (low_period + high_period);
    printf("[AUTOBAUD] Calibrated RTC freq: %u Hz. Calculated baud rate: %u\n", (unsigned int)rtc_freq, (unsigned int)calculated_baud);
    
    uint32_t snapped_baud = snap_to_standard_baud(calculated_baud);
    if (snapped_baud > 0) {
        printf("[AUTOBAUD] Success: Snapped to standard %u bps\n", (unsigned int)snapped_baud);
        ab_detected_baud = snapped_baud;
        ab_state = AB_STATE_SUCCESS;
    } else {
        // Try other formulas in case of signal asymmetry (e.g. only low or only high pulse is reliable)
        if (pos_period > 0 && pos_period < 4095) {
            calculated_baud = rtc_freq * 2 / pos_period;
            snapped_baud = snap_to_standard_baud(calculated_baud);
            if (snapped_baud > 0) {
                printf("[AUTOBAUD] Success (Pos period): Snapped to standard %u bps\n", (unsigned int)snapped_baud);
                ab_detected_baud = snapped_baud;
                ab_state = AB_STATE_SUCCESS;
                return;
            }
        }
        
        if (neg_period > 0 && neg_period < 4095) {
            calculated_baud = rtc_freq * 2 / neg_period;
            snapped_baud = snap_to_standard_baud(calculated_baud);
            if (snapped_baud > 0) {
                printf("[AUTOBAUD] Success (Neg period): Snapped to standard %u bps\n", (unsigned int)snapped_baud);
                ab_detected_baud = snapped_baud;
                ab_state = AB_STATE_SUCCESS;
                return;
            }
        }
        
        printf("[AUTOBAUD] Failed: Calculated baud rate %u could not be snapped to standard rate\n", (unsigned int)calculated_baud);
        ab_state = AB_STATE_FAILED;
    }
}
