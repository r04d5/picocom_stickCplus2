#pragma once

#include <stdbool.h>

// Wi-Fi Bridge global state variables
extern bool wifi_bridge_running;
extern bool client_connected;
extern char client_ip_str[16];

// Function prototypes
void start_wifi_bridge();
void stop_wifi_bridge();
