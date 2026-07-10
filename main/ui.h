#pragma once

#include <stdint.h>
#include <stdbool.h>

// Device Mode Definition
enum DeviceMode {
    MODE_TEXT_TERMINAL,
    MODE_HEX_VIEWER,
    MODE_AUTO_BAUD,
    MODE_SPAMMER,
    MODE_WIFI_BRIDGE,
    MODE_SCANNER
};
extern DeviceMode current_mode;

// Control Menu variables
extern bool in_menu;
extern int menu_selection;
extern const int MENU_ITEMS_COUNT;
extern const char* menu_items[];

// UI Drawing functions
void draw_dashboard_static();
void draw_dashboard_dynamic();
void draw_terminal();
void draw_menu();
