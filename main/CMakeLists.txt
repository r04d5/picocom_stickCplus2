idf_component_register(
    SRCS "main.cpp" "uart_manager.cpp" "terminal.cpp" "autobaud.cpp" "ui.cpp" "spammer.cpp" "wifi_bridge.cpp" "scanner.cpp" "proto_analyzer.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver m5unified esp_wifi
)
