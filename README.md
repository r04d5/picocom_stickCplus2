# M5 UART Tester & Sniffer (M5StickC Plus/Plus2 & M5Cardputer)

This project turns your ESP32/ESP32-S3 M5 Stack device (such as the M5StickC, M5StickC Plus, M5StickC Plus2, or M5Cardputer) into a portable, interactive UART tester, wireless sniffer, and hardware pentesting tool. It is ideal for validating serial connections, debugging custom protocols, capturing unknown baud rates, interrupting bootloader sequences, and performing hardware security audits on 3.3V serial lines directly on the device screen.

The firmware features **automatic hardware detection at runtime**, allowing the **exact same code** to compile and run on both the M5StickC Plus2 and the M5Cardputer, automatically adjusting the physical UART pins and the navigation controls!

---

## 🔌 Physical Connections & Wiring

M5 devices expose their serial communication lines via the **Grove** port:
*   **M5StickC / Plus / Plus2**: TX is `GPIO32`, RX is `GPIO33`.
*   **M5Cardputer**: TX is `GPIO1`, RX is `GPIO2`.

> [!WARNING]
> The red pin of the Grove connector provides **5V**. **Do not connect it directly to 3.3V logic targets (e.g. Raspberry Pi)**, as their GPIOs operate only at 3.3V and can be damaged. Power the M5 device via its own USB-C cable and only wire the data pins (TX/RX) and the common ground (GND).

Connect the pins crosswise (TX to the target's RX, RX to the target's TX):

| Target (e.g. Raspberry Pi) | M5StickC (Grove Port) | M5Cardputer (Grove Port) | Function |
| :--- | :--- | :--- | :--- |
| **TXD / GPIO14** | **GPIO33 / RX** (Blue) | **GPIO2 / RX** (Blue) | Receive (RX) on M5 |
| **RXD / GPIO15** | **GPIO32 / TX** (Yellow) | **GPIO1 / TX** (Yellow) | Transmit (TX) from M5 |
| **GND** | **GND** (Black) | **GND** (Black) | Common reference ground |

---

## 📖 How UART Works

**UART** (Universal Asynchronous Receiver-Transmitter) is a simple physical serial communication protocol. It is **asynchronous**, meaning no clock line is shared between the devices. Instead, both devices must agree on timing parameters beforehand.

### 1. Frame Format
A typical UART data packet (frame) consists of:
*   **Start Bit**: A transition from high (idle) to low logic level indicating data transmission.
*   **Data Bits**: 5 to 9 bits (typically 8 bits) representing the actual payload character.
*   **Parity Bit**: An optional bit used for error detection.
*   **Stop Bit(s)**: 1 or 2 bits pulled high to indicate the end of the frame.

### 2. Baud Rate & Timing
The **Baud Rate** is the transmission speed measured in bits per second (bps). Because there is no shared clock:
*   The receiver samples the line at the middle of each bit period based on its internal timer.
*   If the clock speed of the sender and receiver differs by more than **3%**, the framing synchronization breaks, leading to corrupted data (garbage characters or framing errors).

### 3. Voltage Levels (TTL vs RS-232)
*   **TTL Serial (used by M5/ESP32)**: High logic (1) is 3.3V, Low logic (0) is 0V.
*   **RS-232 (used by PC DB9 ports)**: High logic (1) is -3V to -15V, Low logic (0) is +3V to +15V. Connecting RS-232 directly to the M5 device without a level shifter (e.g. MAX3232) will damage the microcontroller.

---

## 🛡️ Hardware Pentesting with UART

In hardware security audits and pentesting, UART is the most commonly targeted interface because it acts as the primary "backdoor" for developers. 

### Why Pentesters Target UART
1.  **Exposing Linux Root Shells**: Many embedded devices (routers, IoT gateways, smart cameras) leave an unauthenticated serial console active on the board. Finding the TX/RX pins often yields direct root access to the underlying OS.
2.  **Verbose Boot Logs**: During startup, bootloaders (like U-Boot) and the Linux kernel print verbose logs to the UART. These logs expose system information, memory maps, flash partitions, kernel modules, and sometimes even encryption keys or passwords printed in plaintext.
3.  **Bootloader Interruption**: By sending keypresses (like a spacebar or `ctrl+c`) at precise times during boot, an auditor can interrupt the boot sequence. In U-Boot, this drops the system to a command shell. From here, auditors can:
    *   Modify kernel parameters (e.g., adding `init=/bin/sh` to bypass login screens).
    *   Read or dump firmware from SPI Flash memory.
    *   Inject custom payloads or boot an alternate unsigned kernel.

### Security Best Practices (Countermeasures)
*   **Silence the Console**: Disable console output in production bootloaders and kernels.
*   **Secure U-Boot**: Enable `CONFIG_AUTOBOOT_KEYED` or password-protect the bootloader interface.
*   **Physical Protection**: Remove test points, cut traces, or remove serial current-limiting resistors on the production PCB to make physical connections difficult.
*   **Secure Boot**: Cryptographically sign the kernel and enforce signature verification so that boot arguments cannot be altered to load unverified code.

---

## 🎮 Interface & Interactive Controls

Upon boot, the device presents a **Mode Selection Menu**. Controls adapt automatically based on the board you are running:

### Navigation Controls

| Action | M5StickC Controls | M5Cardputer Controls (BTN G0) |
| :--- | :--- | :--- |
| **Cycle / Select Next** | Click **Button A** | Click **Button G0** |
| **Confirm / Action** | Click **Button B** | Press and Hold **Button G0** |
| **Open/Close Menu (Exit Mode)**| Double-click **Button B** | Double-click **Button G0** |
| **Reset Terminal/Stats** | Hold **Button A** | *(Double-click G0 to exit and re-enter)* |

---

### 1. Text Terminal (Interactive Terminal)
Displays standard ASCII/UTF-8 serial traffic in high-visibility terminal green.
*   **Select Next**: Cycles active baud rate: `9600` -> `19200` -> `38400` -> `57600` -> `115200` -> `230400` -> `460800` -> `921600`.
*   **Confirm/Action**: Toggles **Echo Mode** (automatically bounces received characters back to the target).
*   **StickC BtnB Hold**: Transmits a test string packet (`\r\n[StickC-Plus2 UART Test Packet]\r\n`).

### 2. Hex Viewer
Displays NMEA packets, Modbus commands, or raw binary streams formatted as hexadecimal blocks (`ADDR: XX XX XX XX XX XX XX XX | ascii`).
*   **Controls**: Same controls as the Text Terminal.
*   **Use Case**: Essential for debugging binary sensors or auditing unknown protocols where non-printable control characters would mess up standard terminal layouts.

### 3. Auto-Baudrate
Identifies the baud rate of an unknown serial line. It offers **two complementary methods**:

*   **Pulse scan (Select Next / Button A)**: uses the ESP32 hardware auto-baudrate subsystem to measure RX pulse timings and snap to the closest standard rate. Fast, but needs clean edges — works best when the target sends a repeating character (e.g. `U` / `0x55`).
*   **Baud sweep (Confirm / Button B)**: passive brute-force. Reconfigures the UART across all 8 standard rates (~350 ms each) and scores every rate by how much readable ASCII the target emits, picking the rate with the most printable text. This locks onto any console/boot-log output even when the pulse method cannot (no clean edges), and shows live progress (`Testing: 57600 bps (4/8)`, `Best so far: ...`).

*   **Confirm/Action (on Success)**: Applies the detected baud rate and redirects to the Text Terminal.
*   **Use Case**: Connect to an unknown serial line (like a router console), boot the target, and automatically identify the baud rate without trial-and-error — pulse scan if you can inject `U`, baud sweep if you can only listen to the boot log.

### 4. Spammer / Macros
Automates periodic transmissions of preconfigured macros.
*   **Select Next**: Cycles between active macro templates:
    1.  `U-Boot Interrupt`: Spams spacebar `' '` every 15ms.
    2.  `AT Attention`: Sends `"AT\r\n"` every 500ms (verifies modem states).
    3.  `Modbus RTU Poll`: Sends reading frame `\x01\x03\x00\x00\x00\x01\x84\x0A` every 1000ms.
    4.  `GPS Query`: Sends `"$EGPQ,RMC*3C\r\n"` every 2000ms.
    5.  `Loopback Ping`: Sends `"PING\n"` every 250ms (ideal for testing).
*   **Confirm/Action**: Start / Stop periodic transmissions.
*   **Use Case**: Stop bootloaders (e.g. U-Boot) instantly on startup to access recovery shells or test device response to recurring AT/Modbus requests.

### 5. Wi-Fi Bridge (Wireless Sniffer)
Spins up a local Wi-Fi Access Point (`M5-UART-Bridge`) and binds a TCP server to port `8080`.
*   **Use Case**: 
    1.  Connect your laptop to the Wi-Fi network `M5-UART-Bridge`.
    2.  Open a terminal on your laptop and run:
        ```bash
        nc 192.168.4.1 8080
        ```
    3.  Any bytes read by the M5 RX pin are wirelessly printed on your PC, and any command typed in your PC terminal is sent to the target RX pin over Wi-Fi.

### 6. Security Scanner
Automatically inspects the RX stream as it flows, turning manual reconnaissance (listening and reading byte by byte) into automatic identification of exposures. It runs passively across all modes and consolidates the results on this screen.

*   **Security Findings (secret/shell detection)**: reassembles the stream into lines and classifies each line into four categories, keeping a per-category counter and the last matched line:
    *   `SHELL`: exposed console/auth prompts (`login:`, `root@`, `/bin/sh`, `busybox`, `#`/`$`/`=>` prompts...).
    *   `CREDS`: plaintext secret material (`psk=`, `passphrase`, `ssid=`, `token=`, `bearer `, `api_key`...).
    *   `KEYS`: private and API keys (`-----BEGIN`, `ssh-rsa`, `aws_secret`...).
    *   `BOOT`: bootloader banners and interrupt windows (`U-Boot`, `Hit any key`, `starting kernel`...).
*   **Protocol Analysis (macro response validation)**: correlates the target's replies with the Spammer macros, **validating checksums/CRCs** to tell a well-formed reply from a malformed / unchecked one:
    *   `AT`: detects `OK` and `ERROR`/`+CME`.
    *   `NMEA`: validates the `*XX` checksum of `$GP...` sentences (flags invalid or missing CRC — the classic insecure-parsing test).
    *   `Modbus RTU`: verifies the CRC-16 of binary frames and flags frames with a broken CRC.
*   **Reset (StickC)**: hold **Button A** to clear all counters.
*   **Use Case**: leave the scanner running during the target's boot — if a root shell, a Wi-Fi password, or a private key shows up in the log, the matching counter lights up immediately, without you reading the whole dump. A live `Resp:` indicator also appears on the Spammer screen to correlate each sent macro with the reply received.

### 7. Parser Fuzzer
Sends deliberately malformed payloads to the target and watches the RX line for liveness — the practical form of Phase 3 in the audit methodology. A parser that was answering and then goes silent under malformed input is a strong signal of a buffer overflow / hang / DoS (guide §8.3, §8.7, §8.8).

*   **Select Next**: Cycles the fuzz case:
    1.  `NMEA field overflow`: a `$GPGGA,` sentence with a 220-byte field (fixed-buffer probe).
    2.  `AT command overflow`: `AT` followed by 200 bytes (line-buffer probe).
    3.  `NMEA bad checksum`: a well-formed sentence with a wrong `*XX` (does the target act on it anyway?).
    4.  `Ctrl-byte flood`: `0x00 0x11 0x13 0xFF` repeated (NUL + XON/XOFF flow-control abuse).
    5.  `Delimiter storm`: repeated framing chars with empty fields.
    6.  `Modbus broken frame`: a frame claiming a huge byte count, truncated, with a bad CRC.
*   **Confirm/Action**: Start / Stop fuzzing the selected case (every 250 ms).
*   **Target health**: derived from RX activity — `RESPONSIVE` (green), `SILENT...` (yellow, brief gap) or `HANG/DoS!` (red: the target replied earlier and has now been silent for over 5 s under fuzzing). `no reply yet` means the target never spoke, so the result is inconclusive.
*   **Reset (StickC)**: hold **Button A** to clear counters and the liveness baseline.

> [!WARNING]
> Fuzzing can crash or corrupt the target device. Only use it on hardware you own or are authorized to test.

### Line Health Indicator (all modes)
The status bar shows an always-on `LN:OK` / `LN:LOW!` indicator. A healthy idle TTL UART line rests HIGH; a line held LOW for a sustained period (short, jammed transmitter, or a continuous break condition) is a line-level DoS signal (guide §8.8) and turns the indicator red across every mode. A weak internal pull-up on RX keeps a disconnected line reading `LN:OK`, so only an actively driven-low line trips it.

---

## 🔄 Cross-Testing: M5StickC vs M5Cardputer

You can use the M5StickC and the M5Cardputer running this exact firmware to test each other directly:

1.  **Connect the Grove Port Pins Crosswise**:
    *   StickC **TX (G32)** to Cardputer **RX (G2)**.
    *   StickC **RX (G33)** to Cardputer **TX (G1)**.
    *   StickC **GND** to Cardputer **GND**.
2.  **Enable Spammer on StickC**:
    *   Go to **Spammer / Macros** on the StickC.
    *   Select the `Loopback Ping` profile (sends `"PING\n"` every 250ms).
    *   Press **Button B** on the StickC to start sending.
3.  **Observe on Cardputer**:
    *   Go to **Text Terminal** or **Hex Viewer** on the Cardputer.
    *   You will see `"PING"` printing on the screen and the **RX** counter incrementing.
4.  **Test Echo Mode**:
    *   Hold the **G0** button on the Cardputer to activate **ECHO ON**.
    *   Look at the StickC screen: the **RX** counter on the StickC will start incrementing rapidly as the Cardputer bounces the ping packets back!

This setup verifies 100% of the functionalities: TX transmission, RX queue processing, baud rate negotiation, stat counters, and UI redraw logic.

---

## 🛠️ How to Build and Flash

The project supports the **ESP32** family (for M5StickC/Plus/Plus2) and the **ESP32-S3** family (for M5Cardputer). All external libraries are managed automatically by the ESP Component Manager.

1. Open a terminal in the root directory.
2. Activate the ESP-IDF environment:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```
3. Set your compilation target:
   *   For **M5StickC Plus2**: `idf.py set-target esp32` (or `esp32s3` depending on chip variant).
   *   For **M5Cardputer**: `idf.py set-target esp32s3`.
4. Build the project:
   ```bash
   idf.py build
   ```
5. Flash the firmware:
   ```bash
   idf.py -p /dev/ttyACM0 flash monitor
   ```
