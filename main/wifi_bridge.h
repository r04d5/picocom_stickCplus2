#pragma once

#include <stdbool.h>

// Wi-Fi Bridge global state variables
extern bool wifi_bridge_running;
extern bool client_connected;
extern char client_ip_str[16];

// Access Point credentials. The AP is WPA2-PSK protected so an unauthenticated
// bystander cannot join and inject into the target's UART. Change the password
// for your own deployments (must be >= 8 characters).
extern const char* BRIDGE_AP_SSID;
extern const char* BRIDGE_AP_PASSWORD;

// Function prototypes
void start_wifi_bridge();
void stop_wifi_bridge();
