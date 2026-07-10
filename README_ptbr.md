# M5 UART Tester & Sniffer (M5StickC Plus/Plus2 & M5Cardputer)

Este projeto transforma o seu dispositivo M5 baseado em ESP32/ESP32-S3 (como M5StickC, M5StickC Plus, M5StickC Plus2 ou M5Cardputer) em um testador de UART portátil e interativo, sniffer sem fio e ferramenta de hardware pentesting. É ideal para validar conexões seriais de bancada, depurar protocolos personalizados, capturar taxas de baud rate desconhecidas, interromper sequências de bootloaders e realizar auditorias de segurança física em linhas seriais de 3.3V diretamente na tela do dispositivo.

O firmware possui **detecção automática de hardware em tempo de execução**, permitindo que o **exatamente mesmo código** seja compilado e executado tanto no M5StickC Plus2 quanto no M5Cardputer, ajustando automaticamente os pinos físicos da UART e os controles de navegação!

---

## 🔌 Conexões Físicas e Fiação

Os dispositivos M5 expõem suas linhas de comunicação serial através da porta **Grove**:
*   **M5StickC / Plus / Plus2**: TX é `GPIO32`, RX é `GPIO33`.
*   **M5Cardputer**: TX é `GPIO1`, RX é `GPIO2`.

> [!WARNING]
> O pino vermelho do conector Grove fornece **5V**. **Não o conecte diretamente a alvos com lógica de 3.3V (como o Raspberry Pi)**, pois as GPIOs do Pi operam apenas a 3.3V e podem ser danificadas. Alimente o dispositivo M5 via cabo USB-C próprio e faça a fiação apenas para os pinos de dados (TX/RX) e o terra comum (GND).

Conecte os pinos cruzados (TX no RX do outro, RX no TX do outro):

| Alvo (ex: Raspberry Pi) | M5StickC (Porta Grove) | M5Cardputer (Porta Grove) | Função |
| :--- | :--- | :--- | :--- |
| **TXD / GPIO14** | **GPIO33 / RX** (Azul) | **GPIO2 / RX** (Azul) | Recebimento (RX) no M5 |
| **RXD / GPIO15** | **GPIO32 / TX** (Amarelo) | **GPIO1 / TX** (Amarelo) | Transmissão (TX) do M5 |
| **GND** | **GND** (Preto) | **GND** (Preto) | Terra de referência comum |

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

## 🎮 Interface e Controles Interativos

Ao ligar o dispositivo, ele apresenta o **Menu de Seleção de Modo**. Os controles adaptam-se automaticamente ao hardware detectado:

### Tabela de Mapeamento de Controles

| Ação | Controles no M5StickC | Controles no M5Cardputer (BTN G0) |
| :--- | :--- | :--- |
| **Ciclar / Próximo Item** | Clique **Botão A** | Clique **Botão G0** |
| **Confirmar / Ação** | Clique **Botão B** | Pressione e Segure **Botão G0** |
| **Abrir/Fechar Menu (Sair)**| Duplo clique **Botão B** | Duplo clique **Botão G0** |
| **Resetar Terminal/Stats** | Segurar **Botão A** | *(Dbl-clique G0 para sair e reentrar)* |

---

### 1. Text Terminal (Terminal de Texto)
Exibe tráfego serial ASCII/UTF-8 em verde de alta visibilidade.
*   **Ciclar / Próximo**: Altera ciclicamente a taxa de baud rate ativa: `9600` -> `19200` -> `38400` -> `57600` -> `115200` -> `230400` -> `460800` -> `921600`.
*   **Confirmar / Ação**: Alterna o **Modo Echo** (devolve instantaneamente pelo TX tudo que chega no pino RX).
*   **StickC Segurar BtnB**: Transmite um pacote de string de teste serial (`\r\n[StickC-Plus2 UART Test Packet]\r\n`).

### 2. Hex Viewer (Visualizador Hexadecimal)
Exibe tráfego de dados binários, pacotes NMEA de GPS ou telemetria Modbus RTU formatados em blocos hexadecimais (`ADDR: XX XX XX XX XX XX XX XX | ascii`).
*   **Controles**: Mesmos controles do Terminal de Texto.
*   **Caso de Uso**: Essencial para depurar sensores industriais ou inspecionar tráfego binário de rede sem poluir a tela com caracteres não imprimíveis.

### 3. Auto-Baudrate (Detecção de Velocidade)
Mede o intervalo de tempo das bordas dos pulsos de RX recebidos e aproxima o cálculo para a taxa padrão de baud rate mais próxima.
*   **Ciclar / Próximo**: Inicia ou Cancela a rotina de varredura (scanning).
*   **Confirmar / Ação (no Sucesso)**: Aplica o baud rate detectado e redireciona automaticamente para o Terminal de Texto.
*   **Caso de Uso**: Conecte a uma linha de console desconhecida, gere tráfego na linha, e capture a velocidade da porta instantaneamente sem adivinhações.

### 4. Spammer / Macros (Envio Automático)
Automatiza o envio periódico de comandos e macros estruturadas pela serial.
*   **Ciclar / Próximo**: Alterna entre os templates de macro configurados:
    1.  `U-Boot Interrupt`: Envia espaço em branco `' '` a cada 15ms.
    2.  `AT Attention`: Envia `"AT\r\n"` a cada 500ms.
    3.  `Modbus RTU Poll`: Envia frame de leitura `\x01\x03\x00\x00\x00\x01\x84\x0A` a cada 1000ms.
    4.  `GPS Query`: Envia `"$EGPQ,RMC*3C\r\n"` a cada 2000ms.
    5.  `Loopback Ping`: Envia `"PING\n"` a cada 250ms (ideal para testes de loopback).
*   **Confirmar / Ação**: Ativa / Desativa a transmissão periódica.
*   **Caso de Uso**: Interrompe loops de boot (como U-Boot) ou testa comportamento de repetidores e consoles seriais.

### 5. Wi-Fi Bridge (Ponte Sem Fio / Tunnel)
Cria um Access Point local chamado `M5-UART-Bridge` e sobe um servidor TCP na porta `8080`.
*   **Caso de Uso**:
    1.  Conecte o seu computador à rede Wi-Fi `M5-UART-Bridge`.
    2.  Abra um terminal no computador e execute o comando:
        ```bash
        nc 192.168.4.1 8080
        ```
    3.  Qualquer dado que chegar no pino RX da placa M5 será impresso via rede no PC, e qualquer comando digitado no terminal do PC será enviado de volta para a serial do alvo sem a necessidade de cabos.

### 6. Security Scanner (Scanner de Segurança)
Analisa **automaticamente** o fluxo RX enquanto ele passa, transformando o reconhecimento manual (ouvir e ler byte a byte) em identificação automática de exposições. Roda de forma passiva em todos os modos e concentra os resultados nesta tela.

*   **Achados de Segurança (detecção de segredos/shell)**: reassembla o fluxo em linhas e classifica cada linha em quatro categorias, mantendo um contador por categoria e a última linha detectada:
    *   `SHELL`: prompts de console/autenticação expostos (`login:`, `root@`, `/bin/sh`, `busybox`, prompts `#`/`$`/`=>`...).
    *   `CREDS`: material sensível em texto claro (`psk=`, `passphrase`, `ssid=`, `token=`, `bearer `, `api_key`...).
    *   `KEYS`: chaves privadas e de API (`-----BEGIN`, `ssh-rsa`, `aws_secret`...).
    *   `BOOT`: banners e janelas de interrupção de bootloader (`U-Boot`, `Hit any key`, `starting kernel`...).
*   **Análise de Protocolo (validação de resposta dos macros)**: correlaciona as respostas do alvo aos macros do Spammer, **validando checksums/CRC** para distinguir uma resposta bem-formada de uma malformada/não verificada:
    *   `AT`: detecta `OK` e `ERROR`/`+CME`.
    *   `NMEA`: valida o checksum `*XX` das sentenças `$GP...` (sinaliza CRC inválido ou ausente — o teste clássico de parsing inseguro).
    *   `Modbus RTU`: confere o CRC-16 dos frames binários e sinaliza frames com CRC quebrado.
*   **Reset (StickC)**: segure **Button A** para zerar todos os contadores.
*   **Caso de Uso**: deixe o scanner rodando durante o boot do alvo — se um shell root, uma senha de Wi-Fi ou uma chave privada aparecer no log, o contador correspondente acende na hora, sem você precisar ler o dump inteiro. Um indicador `Resp:` ao vivo também aparece na tela do Spammer para correlacionar cada macro enviado com a resposta recebida.

### 7. Parser Fuzzer (Fuzzer de Protocolo)
Envia payloads deliberadamente malformados ao alvo e observa a vivacidade da linha RX — a forma prática da Fase 3 da metodologia de auditoria. Um parser que estava respondendo e fica em silêncio sob entrada malformada é forte indício de buffer overflow / travamento / DoS (guia §8.3, §8.7, §8.8).

*   **Ciclar / Próximo**: Alterna o caso de fuzz:
    1.  `NMEA field overflow`: sentença `$GPGGA,` com um campo de 220 bytes (teste de buffer fixo).
    2.  `AT command overflow`: `AT` seguido de 200 bytes (teste de buffer de linha).
    3.  `NMEA bad checksum`: sentença bem-formada com `*XX` errado (o alvo age mesmo assim?).
    4.  `Ctrl-byte flood`: `0x00 0x11 0x13 0xFF` repetidos (NUL + abuso de controle de fluxo XON/XOFF).
    5.  `Delimiter storm`: caracteres de framing repetidos com campos vazios.
    6.  `Modbus broken frame`: frame declarando byte count enorme, truncado, com CRC inválido.
*   **Confirmar / Ação**: Inicia / Para o fuzzing do caso selecionado (a cada 250 ms).
*   **Saúde do alvo**: derivada da atividade RX — `RESPONSIVE` (verde), `SILENT...` (amarelo, pausa breve) ou `HANG/DoS!` (vermelho: o alvo respondia antes e está em silêncio há mais de 5 s sob fuzzing). `no reply yet` significa que o alvo nunca falou, então o resultado é inconclusivo.
*   **Reset (StickC)**: segure **Button A** para zerar contadores e a linha de base de vivacidade.

> [!WARNING]
> O fuzzing pode travar ou corromper o dispositivo alvo. Use apenas em hardware que você possui ou tem autorização para testar.

---

## 🔄 Teste Cruzado: M5StickC vs M5Cardputer

Você pode usar o M5StickC e o M5Cardputer rodando este firmware exato para testarem um ao outro diretamente:

1.  **Conecte os Pinos da Porta Grove Cruzados**:
    *   StickC **TX (G32)** com Cardputer **RX (G2)**.
    *   StickC **RX (G33)** com Cardputer **TX (G1)**.
    *   StickC **GND** com Cardputer **GND**.
2.  **Inicie o Spammer no StickC**:
    *   Acesse o modo **Spammer / Macros** no StickC.
    *   Selecione a macro `Loopback Ping` (envia `"PING\n"` a cada 250ms).
    *   Pressione o **Botão B** no StickC para iniciar a transmissão.
3.  **Observe no Cardputer**:
    *   Acesse o modo **Text Terminal** ou **Hex Viewer** no Cardputer.
    *   Você verá `"PING"` sendo impresso na tela e o contador **RX** incrementando.
4.  **Teste o Modo Echo**:
    *   Segure o botão **G0** no Cardputer para ativar o **ECHO ON**.
    *   Observe a tela do StickC: o contador **RX** do StickC começará a subir rapidamente à medida que o Cardputer rebate de volta os pacotes de ping recebidos!

Este setup valida 100% das funcionalidades: transmissão TX, processamento da fila RX, negociação de baud rates, contadores de estatísticas e lógica de atualização da tela.

---

## 🛠️ Como Compilar e Gravar

O projeto suporta a família **ESP32** (para M5StickC/Plus/Plus2) e **ESP32-S3** (para M5Cardputer). Todas as dependências externas são gerenciadas automaticamente pelo ESP Component Manager.

1. Abra o terminal na pasta raiz do repositório.
2. Ative o ambiente do ESP-IDF:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```
3. Defina o hardware alvo:
   *   Para **M5StickC Plus2**: `idf.py set-target esp32` (ou `esp32s3` dependendo do variante).
   *   Para **M5Cardputer**: `idf.py set-target esp32s3`.
4. Compile o projeto:
   ```bash
   idf.py build
   ```
5. Grave o firmware:
   ```bash
   idf.py -p /dev/ttyACM0 flash monitor
   ```
