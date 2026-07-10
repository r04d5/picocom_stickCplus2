#include "uart_manager.h"
#include <driver/gpio.h>
#include <M5Unified.h>

gpio_num_t get_grove_tx_pin() {
    auto b = M5.getBoard();
    if (b == m5::board_t::board_M5Cardputer || b == m5::board_t::board_M5CardputerADV) {
        return GPIO_NUM_1;
    }
    return GPIO_NUM_32;
}

gpio_num_t get_grove_rx_pin() {
    auto b = M5.getBoard();
    if (b == m5::board_t::board_M5Cardputer || b == m5::board_t::board_M5CardputerADV) {
        return GPIO_NUM_2;
    }
    return GPIO_NUM_33;
}

// Baud rate configuration
uint32_t baud_rates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
int baud_idx = 4; // Initialize at 115200
uint32_t current_baud = 115200;

// Statistics and State
uint32_t rx_bytes = 0;
uint32_t tx_bytes = 0;
bool echo_mode = false;

// Queue for communication between the RX Task and the Main loop
QueueHandle_t rx_queue = NULL;
volatile bool uart_rx_enabled = true;

// Task to read from UART and put in the Queue asynchronously
void uart_rx_task(void *pvParameters) {
    uint8_t data[128];
    while (1) {
        if (!uart_rx_enabled) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        // Read available bytes on UART1
        int len = uart_read_bytes(UART_NUM_1, data, sizeof(data), pdMS_TO_TICKS(10));
        for (int i = 0; i < len; i++) {
            if (rx_queue != NULL) {
                xQueueSend(rx_queue, &data[i], portMAX_DELAY);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void init_uart(uint32_t baud_rate) {
    if (uart_is_driver_installed(UART_NUM_1)) {
        uart_driver_delete(UART_NUM_1);
    }

    uart_config_t uart_config = {};
    uart_config.baud_rate = (int)baud_rate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 122;

    // Configure basic parameters
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));

    // Define physical pins dynamically based on the board type
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, get_grove_tx_pin(), get_grove_rx_pin(), UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Weak pull-up on RX so a disconnected/floating line idles HIGH (matching a
    // real idle UART line). This prevents the line-anomaly monitor from false-
    // flagging an unconnected pin, so only a driven low reads as stuck.
    gpio_pullup_en(get_grove_rx_pin());

    // Install driver with 1024-byte buffers
    const int uart_buffer_size = 1024;
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, uart_buffer_size, uart_buffer_size, 0, NULL, 0));
}
