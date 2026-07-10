idf_component_register(
    SRCS "main.cpp" "uart_manager.cpp" "terminal.cpp" "autobaud.cpp" "ui.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver m5unified
)
