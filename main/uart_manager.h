#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Baud rate configuration
extern uint32_t baud_rates[];
extern int baud_idx;
extern uint32_t current_baud;

// Statistics and State
extern uint32_t rx_bytes;
extern uint32_t tx_bytes;
extern bool echo_mode;

// Queue for communication between the RX Task and the Main loop
extern QueueHandle_t rx_queue;
extern volatile bool uart_rx_enabled;

#include <driver/gpio.h>

// Function prototypes
gpio_num_t get_grove_tx_pin();
gpio_num_t get_grove_rx_pin();
void init_uart(uint32_t baud_rate);
void uart_rx_task(void *pvParameters);
