# M5 UART Tester (picocom / sniffer / pentesting)

Este projeto transforma o seu dispositivo M5 baseado em ESP32 (como M5StickC, M5StickC Plus ou M5StickC Plus2) em um testador de UART portátil e interativo, sniffer sem fio e ferramenta de hardware pentesting. É ideal para validar conexões seriais de bancada, depurar protocolos personalizados, capturar taxas de baud rate desconhecidas, interromper sequências de bootloaders e realizar auditorias de segurança física em linhas seriais de 3.3V diretamente na tela do dispositivo.

O projeto foi desenvolvido nativamente utilizando o framework oficial **ESP-IDF** e a biblioteca **M5Unified**.

---

## 🔌 Conexões Físicas (Fiação)

O conector lateral **Grove** do dispositivo M5 expõe os pinos GPIO32 e GPIO33.

> [!WARNING]
> O pino vermelho do conector Grove fornece **5V**. **Não o conecte diretamente a alvos com lógica de 3.3V (como o Raspberry Pi)**, pois as GPIOs do Pi operam apenas a 3.3V e podem ser danificadas. Alimente o dispositivo M5 via cabo USB-C próprio e faça a fiação apenas para os pinos de dados (TX/RX) e o terra comum (GND).

Conecte os pinos cruzados (TX no RX do outro, RX no TX do outro):

| Alvo (ex: Raspberry Pi) | Dispositivo M5 (Porta Grove) | Fio | Função |
| :--- | :--- | :--- | :--- |
| **TXD / GPIO14** (Pino 8) | **GPIO33 / RX** | Azul | Recebimento (RX) no M5 |
| **RXD / GPIO15** (Pino 10) | **GPIO32 / TX** | Amarelo | Transmissão (TX) do M5 |
| **GND** (Pino 6 ou 9) | **GND** | Preto | Terra de referência comum |

---

## 📖 Como Funciona a UART

A **UART** (Universal Asynchronous Receiver-Transmitter) é um protocolo físico simples de comunicação serial. Ela é **assíncrona**, o que significa que nenhuma linha de clock é compartilhada entre os dispositivos. Em vez disso, ambos os dispositivos devem concordar previamente com os parâmetros de temporização.

### 1. Formato do Frame (Quadro)
Um pacote típico de dados UART consiste em:
*   **Start Bit (Bit de Início)**: Uma transição do nível lógico alto (idle/repouso) para baixo, indicando o início da transmissão.
*   **Data Bits (Bits de Dados)**: De 5 a 9 bits (geralmente 8 bits) representando o caractere de payload real.
*   **Parity Bit (Bit de Paridade)**: Um bit opcional usado para detecção de erros básicos de integridade.
*   **Stop Bit(s) (Bit(s) de Parada)**: 1 ou 2 bits puxados em nível lógico alto para indicar o fim do frame.

### 2. Baud Rate e Temporização
O **Baud Rate** é a velocidade de transmissão medida em bits por segundo (bps). Como não há clock compartilhado:
*   O receptor amostra o estado da linha no ponto médio de cada período de bit com base no seu timer interno.
*   Se a diferença de velocidade de clock entre o transmissor e o receptor for maior que **3%**, a sincronização de amostragem falha, gerando corrupção de dados (caracteres incompreensíveis ou erros de frame).

### 3. Níveis de Tensão (TTL vs RS-232)
*   **Serial TTL (usado por M5/ESP32)**: Nível lógico alto (1) é 3.3V, Nível lógico baixo (0) é 0V.
*   **RS-232 (usado em portas DB9 de PCs)**: Nível lógico alto (1) é -3V a -15V, Nível lógico baixo (0) é +3V a +15V. Conectar portas RS-232 diretamente ao dispositivo M5 sem um conversor de nível (ex: MAX3232) danificará permanentemente o microcontrolador.

---

## 🛡️ Pentesting de Hardware com UART

Em auditorias de segurança de hardware e pentesting, a UART é uma das interfaces mais visadas por ser a principal "porta dos fundos" deixada por engenheiros e desenvolvedores.

### Por que a UART é visada em Auditorias
1.  **Exposição de Consolas Root**: Muitos dispositivos embarcados (roteadores, IoT gateways, câmeras IP) deixam consoles seriais ativos sem qualquer tipo de autenticação por senha. Achar os pinos de TX/RX frequentemente concede acesso root instantâneo ao terminal do sistema operacional (geralmente Linux).
2.  **Logs de Boot Detalhados**: Durante a inicialização, bootloaders (como U-Boot) e o kernel Linux imprimem logs verbosos sobre a UART. Estes logs revelam arquitetura do processador, partições da memória flash, sistema de arquivos em uso, chaves criptográficas decodificadas e senhas impressas em texto claro.
3.  **Interrupção da Inicialização**: Ao enviar caracteres específicos (como barra de espaço ou `ctrl+c`) no momento exato em que a placa liga, um auditor pode pausar o bootloader. No console do U-Boot, isso permite:
    *   Alterar argumentos do kernel (como adicionar `init=/bin/sh` para pular telas de login).
    *   Ler e extrair o conteúdo da memória SPI Flash (dump de firmware).
    *   Gravar payloads ou dar boot em kernels não assinados na memória RAM.

### Melhores Práticas de Segurança (Contramedidas)
*   **Silenciar o Console**: Desativar saídas seriais em bootloaders e kernels em firmwares de produção.
*   **Proteger o Bootloader**: Habilitar recursos de proteção por senha (como `CONFIG_AUTOBOOT_KEYED` no U-Boot).
*   **Proteção Física**: Remover ilhas de teste de TX/RX expostas, cortar trilhas de comunicação ou remover resistores de acoplamento em série no layout final da PCI de produção.
*   **Secure Boot**: Assinar criptograficamente o kernel e imagens de boot para garantir que qualquer alteração de argumentos via serial impeça a inicialização de código não autorizado.

---

## 🎮 Interface e Modos Interativos

Ao ligar o dispositivo, ele apresenta o **Menu de Seleção de Modo**.

### Controles de Navegação
*   **Botão A (Frontal - M5)**: Alterna entre os modos disponíveis.
*   **Botão B (Lateral Superior)**: Confirma a seleção / Entra no modo.
*   **Botão B (Duplo Clique)**: Pressione a qualquer momento dentro de um modo para sair e retornar ao menu principal.

---

### 1. Text Terminal (Terminal de Texto)
Exibe tráfego serial ASCII/UTF-8 em verde de alta visibilidade.
*   **Botão A (Clique)**: Altera ciclicamente a taxa de baud rate ativa: `9600` -> `19200` -> `38400` -> `57600` -> `115200` -> `230400` -> `460800` -> `921600`.
*   **Botão A (Segurar)**: Limpa a tela do terminal e zera os contadores RX/TX.
*   **Botão B (Clique)**: Alterna o **Modo Echo** (devolve instantaneamente pelo TX tudo que chega no pino RX).
*   **Botão B (Segurar)**: Transmite um pacote de string de teste serial (`\r\n[StickC-Plus2 UART Test Packet]\r\n`).

### 2. Hex Viewer (Visualizador Hexadecimal)
Exibe tráfego de dados binários, pacotes NMEA de GPS ou telemetria Modbus RTU formatados em blocos hexadecimais (`ADDR: XX XX XX XX XX XX XX XX | ascii`).
*   **Botões A e B**: Possuem os mesmos controles do Terminal de Texto.
*   **Caso de Uso**: Essencial para depurar sensores industriais ou inspecionar tráfego binário de rede sem poluir a tela com caracteres não imprimíveis.

### 3. Auto-Baudrate (Detecção de Velocidade)
Mede o intervalo de tempo das bordas dos pulsos de RX recebidos e aproxima o cálculo para a taxa padrão de baud rate mais próxima.
*   **Botão A**: Inicia ou Cancela a rotina de varredura (scanning).
*   **Botão B (Clique Simples após Sucesso)**: Aplica o baud rate detectado e redireciona automaticamente para o Terminal de Texto.
*   **Caso de Uso**: Conecte a uma linha de console desconhecida (como um roteador), ligue a placa-alvo para gerar tráfego de boot, e capture a velocidade da porta instantaneamente sem adivinhações.

### 4. Spammer / Macros (Envio Automático)
Automatiza o envio periódico de comandos e macros estruturadas pela serial.
*   **Botão A (Clique)**: Alterna entre os templates de macro configurados:
    1.  `U-Boot Interrupt`: Envia espaço em branco `' '` a cada 15ms.
    2.  `AT Attention`: Envia `"AT\r\n"` a cada 500ms (testa status de modems).
    3.  `Modbus RTU Poll`: Envia frame de leitura `\x01\x03\x00\x00\x00\x01\x84\x0A` a cada 1000ms.
    4.  `GPS Query`: Envia `"$EGPQ,RMC*3C\r\n"` a cada 2000ms.
*   **Botão B (Clique)**: Ativa / Desativa a transmissão periódica.
*   **Caso de Uso**: Interrompe loops de boot (como U-Boot) enviando comandos em milissegundos assim que o hardware alvo é alimentado, ou testa comportamento de repetidores Modbus.

### 5. Wi-Fi Bridge (Ponte Sem Fio / Tunnel)
Cria um Access Point local chamado `M5-UART-Bridge` e sobe um servidor TCP na porta `8080`.
*   **Caso de Uso**:
    1.  Conecte o seu computador à rede Wi-Fi `M5-UART-Bridge`.
    2.  Abra um terminal no computador e execute o comando:
        ```bash
        nc 192.168.4.1 8080
        ```
    3.  Qualquer dado que chegar no pino RX da placa M5 será impresso via rede no PC, e qualquer comando digitado no terminal do PC será enviado de volta para a serial do alvo sem a necessidade de cabos.

---

## 🛠️ Como Compilar e Gravar

O projeto é voltado à família **ESP32**. Todas as bibliotecas externas necessárias são baixadas via ESP Component Manager.

1. Abra o terminal na pasta raiz do repositório.
2. Ative o ambiente do ESP-IDF:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```
3. Defina o hardware alvo (ex: `esp32` ou `esp32s3`):
   ```bash
   idf.py set-target esp32
   ```
4. Compile o projeto:
   ```bash
   idf.py build
   ```
5. Grave o firmware e execute o monitor de logs:
   ```bash
   idf.py -p /dev/ttyACM0 flash monitor
   ```
