# M5StickC Plus2 UART Tester (picocom)

Este projeto transforma o M5StickC Plus2 em um testador de UART portátil e interativo (similar ao utilitário `picocom` / modo echo de hardware). É ideal para validar conexões seriais de bancada (ex: testar a UART de um Raspberry Pi, módulos GPS, sensores industriais ou outros microcontroladores de 3.3V) diretamente pela tela do StickC Plus2.

O projeto foi estruturado utilizando o framework oficial **ESP-IDF** nativo e a biblioteca **M5Unified** gerenciada via ESP Component Manager.

---

## 🔌 Conexões Físicas (Fiação)

O conector lateral **Grove** do M5StickC Plus2 expõe os pinos GPIO32 e GPIO33.

> [!WARNING]
> O pino vermelho do conector Grove do StickC fornece **5V**. **Não o conecte ao Raspberry Pi**, pois as GPIOs do Pi operam apenas a 3.3V e podem ser danificadas. Alimente o StickC Plus2 via cabo USB-C próprio e faça a fiação apenas para os pinos de dados (TX/RX) e o terra (GND).

Conecte os pinos cruzados (TX no RX do outro):

| Raspberry Pi (Header) | M5StickC Plus2 (Porta Grove) | Fio | Função |
| :--- | :--- | :--- | :--- |
| **GPIO14 / TXD** (Pino 8) | **GPIO33 / RX** | Azul | Recebimento (RX) no StickC |
| **GPIO15 / RXD** (Pino 10) | **GPIO32 / TX** | Amarelo | Transmissão (TX) do StickC |
| **GND** (Pino 6 ou 9) | **GND** | Preto | Terra de referência comum |

---

## 🎮 Operação e Interface

A interface do dispositivo é desenhada em modo paisagem no display LCD de 240x135 e é dividida em três partes principais:

1. **Header (Topo - Azul)**: Exibe o nome da aplicação, a velocidade de transmissão ativa (ex: `B:115200`) e se o modo de eco está habilitado (`ECHO ON` / `ECHO OFF`).
2. **Stats Panel (Meio - Cinza Escuro)**: Exibe o total de bytes lidos (**RX**) e escritos (**TX**) desde o último reinício.
3. **Terminal Console (Abaixo - Preto)**: Exibe as últimas 11 linhas dos dados recebidos do transmissor em verde de alta visibilidade, com rolagem automática e quebra de linha inteligente.

### Controles Físicos

*   **Botão A (Frontal - M5)**:
    *   **Clique Rápido**: Altera ciclicamente a taxa de baud rate da UART: `9600` -> `19200` -> `38400` -> `57600` -> `115200` -> `230400` -> `460800` -> `921600`. O driver UART é reinstalado de forma limpa a cada troca de velocidade.
    *   **Pressionar e Segurar (0.5s)**: Reseta completamente a tela do terminal e zera os contadores RX/TX.
*   **Botão B (Lateral Superior)**:
    *   **Clique Rápido**: Alterna o modo **Echo** (se ativo, qualquer byte recebido no RX é instantaneamente devolvido pelo TX, ideal para testes de loopback com o terminal do Pi).
    *   **Pressionar e Segurar (0.5s)**: Envia uma string de teste estruturada pela porta serial (`\r\n[StickC-Plus2 UART Test Packet]\r\n`) para depuração no lado receptor.

---

## 🛠️ Como Compilar e Gravar

Como o projeto utiliza o ESP Component Manager integrado com o CMake do ESP-IDF, as dependências (`M5Unified` e `M5GFX`) são baixadas de forma totalmente transparente na primeira compilação.

1. Abra um terminal no diretório raiz do projeto (`picocom_stickCplus2`).
2. Ative o ambiente do ESP-IDF (ex: `. $HOME/esp/esp-idf/export.sh` ou similar dependendo da sua instalação).
3. Compile o projeto:
   ```bash
   idf.py build
   ```
4. Grave o firmware no StickC Plus2 e abra o monitor serial para verificar o log de inicialização:
   ```bash
   idf.py -p /dev/ttyACM0 flash monitor
   ```

---

## 🔍 Estrutura do Repositório

```
├── CMakeLists.txt         # Definição do projeto CMake do ESP-IDF
├── sdkconfig.defaults     # Configuração padrão de Flash de 8MB do StickC Plus2
├── .gitignore             # Arquivos ignorados pelo Git
├── README.md              # Este manual de uso
└── main/
    ├── CMakeLists.txt     # CMake do componente principal
    ├── idf_component.yml  # Manifesto que baixa o M5Unified do ESP Registry
    └── main.cpp           # Lógica do picocom (UART nativa + FreeRTOS queues + UI)
```
