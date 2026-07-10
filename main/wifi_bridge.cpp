#include "wifi_bridge.h"
#include "uart_manager.h"
#include <esp_wifi.h>
#include <esp_mac.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_log.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdio.h>
#include <driver/uart.h>
#include <errno.h>
#include <fcntl.h>

bool wifi_bridge_running = false;
bool client_connected = false;
char client_ip_str[16] = "0.0.0.0";

static TaskHandle_t bridge_task_handle = NULL;
static int server_socket = -1;
static int client_socket = -1;
static esp_netif_t *ap_netif = NULL;

static void wifi_bridge_task(void *pvParameters) {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(8080);

    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (server_socket < 0) {
        printf("[WIFI-B] Failed to create socket\n");
        bridge_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        printf("[WIFI-B] Socket bind failed\n");
        close(server_socket);
        server_socket = -1;
        bridge_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    if (listen(server_socket, 1) != 0) {
        printf("[WIFI-B] Socket listen failed\n");
        close(server_socket);
        server_socket = -1;
        bridge_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    printf("[WIFI-B] TCP Server listening on port 8080\n");

    while (wifi_bridge_running) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&source_addr, &addr_len);
        if (client_socket < 0) {
            if (!wifi_bridge_running) break;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        inet_ntoa_r(source_addr.sin_addr, client_ip_str, sizeof(client_ip_str) - 1);
        printf("[WIFI-B] Client connected: %s\n", client_ip_str);
        client_connected = true;

        int flags = fcntl(client_socket, F_GETFL, 0);
        fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

        char rx_buf[128];
        while (wifi_bridge_running && client_connected) {
            // 1. Read from TCP Client -> Write to UART
            int len = recv(client_socket, rx_buf, sizeof(rx_buf), 0);
            if (len > 0) {
                uart_write_bytes(UART_NUM_1, rx_buf, len);
                tx_bytes += len;
            } else if (len < 0) {
                int err = errno;
                if (err != EWOULDBLOCK && err != EAGAIN) {
                    printf("[WIFI-B] Socket read error: %d, closing client\n", err);
                    client_connected = false;
                    break;
                }
            } else {
                printf("[WIFI-B] Client disconnected\n");
                client_connected = false;
                break;
            }

            // 2. Read from UART RX queue -> Write to TCP Client
            char c;
            while (xQueueReceive(rx_queue, &c, 0) == pdTRUE) {
                rx_bytes++;
                int sent = send(client_socket, &c, 1, 0);
                if (sent < 0) {
                    int err = errno;
                    if (err != EWOULDBLOCK && err != EAGAIN) {
                        printf("[WIFI-B] Socket write error: %d, closing client\n", err);
                        client_connected = false;
                        break;
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(5));
        }

        if (client_socket >= 0) {
            close(client_socket);
            client_socket = -1;
        }
        client_connected = false;
        strcpy(client_ip_str, "0.0.0.0");
    }

    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
    printf("[WIFI-B] Server task exiting\n");
    bridge_task_handle = NULL;
    vTaskDelete(NULL);
}

void start_wifi_bridge() {
    if (wifi_bridge_running) return;

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, "M5-UART-Bridge");
    wifi_config.ap.ssid_len = strlen("M5-UART-Bridge");
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    printf("[WIFI-B] Access Point M5-UART-Bridge started\n");

    wifi_bridge_running = true;
    xTaskCreate(wifi_bridge_task, "wifi_bridge_task", 4096, NULL, 5, &bridge_task_handle);
}

void stop_wifi_bridge() {
    if (!wifi_bridge_running) return;

    wifi_bridge_running = false;

    if (client_socket >= 0) {
        close(client_socket);
        client_socket = -1;
    }
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }

    client_connected = false;
    strcpy(client_ip_str, "0.0.0.0");

    int timeout = 50;
    while (bridge_task_handle != NULL && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(5));
        timeout--;
    }

    esp_wifi_stop();
    esp_wifi_deinit();

    if (ap_netif != NULL) {
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
    }

    printf("[WIFI-B] Access Point and TCP server stopped\n");
}
