#include <M5Unified.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <string.h>

#define MAX_COLS 39
#define MAX_ROWS 11

// Buffer para o terminal
char terminal_buf[MAX_ROWS][MAX_COLS + 1];
int current_row = 0;
int current_col = 0;

// Estatísticas e Estado
uint32_t rx_bytes = 0;
uint32_t tx_bytes = 0;
bool echo_mode = false;

// Configuração de Baud Rate
uint32_t baud_rates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
int baud_idx = 4; // Inicializa em 115200
uint32_t current_baud = 115200;

// Queue para comunicação entre a Task de leitura e a Main loop
QueueHandle_t rx_queue = NULL;

// Protótipos de funções
void draw_dashboard_static();
void draw_dashboard_dynamic();
void draw_terminal();
void init_uart(uint32_t baud_rate);
void add_char_to_terminal(char c);

// Task para ler da UART e colocar na Queue de forma assíncrona
void uart_rx_task(void *pvParameters) {
    uint8_t data[128];
    while (1) {
        // Lê os bytes disponíveis na UART1
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

    // Configura os parâmetros básicos
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));

    // Define os pinos físicos: TX = GPIO32, RX = GPIO33
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, GPIO_NUM_32, GPIO_NUM_33, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Instala o driver com buffers de 1024 bytes
    const int uart_buffer_size = 1024;
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, uart_buffer_size, uart_buffer_size, 0, NULL, 0));
}

void add_char_to_terminal(char c) {
    if (c == '\r') {
        current_col = 0;
        return;
    }
    if (c == '\n') {
        current_row++;
        if (current_row >= MAX_ROWS) {
            // Desloca as linhas para cima
            for (int i = 1; i < MAX_ROWS; i++) {
                strcpy(terminal_buf[i - 1], terminal_buf[i]);
            }
            current_row = MAX_ROWS - 1;
        }
        current_col = 0;
        memset(terminal_buf[current_row], 0, sizeof(terminal_buf[current_row]));
        return;
    }

    // Apenas exibe caracteres imprimíveis padrão
    if (c >= 32 && c <= 126) {
        if (current_col >= MAX_COLS - 1) {
            add_char_to_terminal('\n');
        }
        terminal_buf[current_row][current_col++] = c;
        terminal_buf[current_row][current_col] = '\0';
    }
}

void draw_dashboard_static() {
    // Cabeçalho azul moderno (Header)
    M5.Display.fillRect(0, 0, 240, 18, M5.Display.color565(59, 130, 246));
    M5.Display.setTextColor(WHITE);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);
    M5.Display.drawString("STCP2 PICOCOM", 6, 5);

    // Painel de Estatísticas cinza escuro
    M5.Display.fillRect(0, 18, 240, 14, M5.Display.color565(30, 41, 59));

    // Fundo da Janela do Terminal (preto escuro) e bordas
    M5.Display.fillRect(0, 32, 240, 103, M5.Display.color565(15, 23, 42));
    M5.Display.drawRect(0, 32, 240, 103, M5.Display.color565(51, 65, 85));
}

void draw_dashboard_dynamic() {
    // Atualiza a parte direita do Cabeçalho (Baud Rate e status de Echo)
    M5.Display.fillRect(100, 0, 140, 18, M5.Display.color565(59, 130, 246));

    char baud_str[16];
    snprintf(baud_str, sizeof(baud_str), "B:%u", (unsigned int)current_baud);
    M5.Display.setTextColor(YELLOW);
    M5.Display.drawString(baud_str, 110, 5);

    M5.Display.setTextColor(echo_mode ? GREEN : M5.Display.color565(203, 213, 225));
    M5.Display.drawString(echo_mode ? "ECHO ON" : "ECHO OFF", 180, 5);

    // Atualiza o painel de estatísticas (RX e TX bytes)
    M5.Display.fillRect(0, 18, 240, 14, M5.Display.color565(30, 41, 59));
    
    char rx_str[32];
    snprintf(rx_str, sizeof(rx_str), "RX: %u B", (unsigned int)rx_bytes);
    M5.Display.setTextColor(CYAN);
    M5.Display.drawString(rx_str, 6, 21);

    char tx_str[32];
    snprintf(tx_str, sizeof(tx_str), "TX: %u B", (unsigned int)tx_bytes);
    M5.Display.setTextColor(ORANGE);
    M5.Display.drawString(tx_str, 120, 21);
}

void draw_terminal() {
    // Limpa a área interna do terminal
    M5.Display.fillRect(1, 33, 238, 101, M5.Display.color565(15, 23, 42));
    M5.Display.setTextColor(M5.Display.color565(74, 222, 128)); // Verde Terminal
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);

    for (int i = 0; i < MAX_ROWS; i++) {
        if (strlen(terminal_buf[i]) > 0 || i <= current_row) {
            M5.Display.drawString(terminal_buf[i], 6, 35 + i * 9);
        }
    }
}

extern "C" void app_main() {
    // Inicialização do M5Unified
    auto cfg = M5.config();
    M5.begin(cfg);

    // Configuração de display em modo paisagem
    M5.Display.setRotation(1);

    // Inicializa a tela com o layout básico
    draw_dashboard_static();
    draw_dashboard_dynamic();
    draw_terminal();

    // Inicializa o buffer do terminal com strings vazias
    for (int i = 0; i < MAX_ROWS; i++) {
        memset(terminal_buf[i], 0, sizeof(terminal_buf[i]));
    }

    // Inicializa a UART1 com o baud rate padrão (115200)
    init_uart(current_baud);

    // Cria a Queue para buffering de entrada UART (tamanho de 1024 caracteres)
    rx_queue = xQueueCreate(1024, sizeof(char));

    // Inicia a Task de RX de UART no núcleo 0
    xTaskCreatePinnedToCore(uart_rx_task, "uart_rx_task", 4096, NULL, 5, NULL, 0);

    bool terminal_dirty = false;
    uint32_t last_draw_time = 0;

    while (1) {
        // Atualiza leitura de botões
        M5.update();

        // Botão Frontal (BtnA)
        if (M5.BtnA.wasClicked()) {
            baud_idx = (baud_idx + 1) % (sizeof(baud_rates) / sizeof(baud_rates[0]));
            current_baud = baud_rates[baud_idx];
            init_uart(current_baud);
            draw_dashboard_dynamic();
        } else if (M5.BtnA.wasHold()) {
            // Pressionar e segurar: Reset completo da tela e contadores
            for (int i = 0; i < MAX_ROWS; i++) {
                memset(terminal_buf[i], 0, sizeof(terminal_buf[i]));
            }
            current_row = 0;
            current_col = 0;
            rx_bytes = 0;
            tx_bytes = 0;
            draw_dashboard_dynamic();
            draw_terminal();
        }

        // Botão Lateral (BtnB)
        if (M5.BtnB.wasClicked()) {
            echo_mode = !echo_mode;
            draw_dashboard_dynamic();
        } else if (M5.BtnB.wasHold()) {
            // Pressionar e segurar: Envia um pacote de testes pela serial
            const char *test_msg = "\r\n[StickC-Plus2 UART Test Packet]\r\n";
            int len = strlen(test_msg);
            uart_write_bytes(UART_NUM_1, test_msg, len);
            tx_bytes += len;
            draw_dashboard_dynamic();
        }

        // Processa todos os caracteres que chegaram na Queue
        char c;
        bool data_received = false;
        while (xQueueReceive(rx_queue, &c, 0) == pdTRUE) {
            rx_bytes++;
            data_received = true;
            add_char_to_terminal(c);

            // Ecoa de volta na UART1 se habilitado
            if (echo_mode) {
                uart_write_bytes(UART_NUM_1, &c, 1);
                tx_bytes++;
            }
        }

        if (data_received) {
            terminal_dirty = true;
        }

        // Taxa limite para atualização da tela (FPS) - Evita lentidão no buffer e flickering
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
