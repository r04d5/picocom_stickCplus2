# M5StickC Plus2 UART Tester (picocom)

This project turns the M5StickC Plus2 into a portable, interactive UART tester (similar to the `picocom` utility / hardware echo mode). It is ideal for validating serial bench connections (e.g., testing a Raspberry Pi's UART, GPS modules, industrial sensors, or other 3.3V microcontrollers) directly on the StickC Plus2 screen.

The project is structured using the official native **ESP-IDF** framework and the **M5Unified** library managed via the ESP Component Manager.

---

## 🔌 Physical Connections (Wiring)

The side **Grove** connector of the M5StickC Plus2 exposes the GPIO32 and GPIO33 pins.

> [!WARNING]
> The red pin of the StickC Grove connector provides **5V**. **Do not connect it to the Raspberry Pi**, as the Pi's GPIOs operate only at 3.3V and can be damaged. Power the StickC Plus2 via its own USB-C cable and only wire the data pins (TX/RX) and the ground (GND).

Connect the pins crosswise (TX to the other's RX):

| Raspberry Pi (Header) | M5StickC Plus2 (Grove Port) | Wire | Function |
| :--- | :--- | :--- | :--- |
| **GPIO14 / TXD** (Pin 8) | **GPIO33 / RX** | Blue | Receive (RX) on StickC |
| **GPIO15 / RXD** (Pin 10) | **GPIO32 / TX** | Yellow | Transmit (TX) from StickC |
| **GND** (Pin 6 or 9) | **GND** | Black | Common reference ground |

---

## 🎮 Operation and Interface

The device interface is drawn in landscape mode on the 240x135 LCD display and is divided into three main parts:

1. **Header (Top - Blue)**: Displays the application name, active baud rate (e.g., `B:115200`), and whether echo mode is enabled (`ECHO ON` / `ECHO OFF`).
2. **Stats Panel (Middle - Dark Gray)**: Displays the total bytes read (**RX**) and written (**TX**) since the last reset.
3. **Terminal Console (Bottom - Black)**: Displays the last 11 lines of data received from the transmitter in high-visibility green, featuring automatic scrolling and smart line wrapping.

### Physical Controls

*   **Button A (Front - M5)**:
    *   **Quick Click**: Cyclically changes the UART baud rate: `9600` -> `19200` -> `38400` -> `57600` -> `115200` -> `230400` -> `460800` -> `921600`. The UART driver is cleanly reinstalled at every speed change.
    *   **Press and Hold (0.5s)**: Fully resets the terminal screen and clears the RX/TX counters.
*   **Button B (Top Side)**:
    *   **Quick Click**: Toggles **Echo** mode (if active, any byte received on RX is instantly sent back through TX, ideal for loopback tests with the Pi's terminal).
    *   **Press and Hold (0.5s)**: Sends a structured test string through the serial port (`\r\n[StickC-Plus2 UART Test Packet]\r\n`) for debugging on the receiving end.

---

## 🛠️ How to Build and Flash

Since the project uses the ESP Component Manager integrated with ESP-IDF's CMake, dependencies (`M5Unified` and `M5GFX`) are downloaded completely transparently during the first build.

1. Open a terminal in the root directory of the project (`picocom_stickCplus2`).
2. Activate the ESP-IDF environment (e.g., `. $HOME/esp/esp-idf/export.sh` or similar, depending on your installation).
3. Build the project:
   ```bash
   idf.py build
   ```
4. Flash the firmware to the StickC Plus2 and open the serial monitor to check the boot log:
   ```bash
   idf.py -p /dev/ttyACM0 flash monitor
   ```

---

## 🔍 Repository Structure

```
├── CMakeLists.txt         # ESP-IDF CMake project definition
├── sdkconfig.defaults     # M5StickC Plus2 default 8MB Flash configuration
├── .gitignore             # Git ignored files
├── README.md              # This usage manual
└── main/
    ├── CMakeLists.txt     # Main component CMake
    ├── idf_component.yml  # Manifesto downloading M5Unified from the ESP Registry
    └── main.cpp           # picocom logic (native UART + FreeRTOS queues + UI)
```

---

## 🗺️ Roadmap (Future Features)

To make this tool even more powerful for embedded systems development and hardware security testing, the following phases are planned:

*   **Phase 1: Multi-Mode Menu & Hex/Binary Viewer** (Completed)
    *   Adds a Mode Switch Menu (double-click Button B).
    *   Adds a live `hexdump` terminal viewer to display non-printable characters.
*   **Phase 2: Auto-Baudrate Detection** (Pending)
    *   Measuring RX pulse edge-timings to calculate and configure the correct baud rate automatically.
*   **Phase 3: Macro Sender & Bootloader Interrupter** (Pending)
    *   Sending custom command packets (Modbus, AT commands).
    *   Automatic keyboard spammer to interrupt bootloaders (e.g. U-Boot) on target power-up.
*   **Phase 4: Wi-Fi bridge (Sniffer Tunnel)** (Pending)
    *   Bridging the physical UART interface to a wireless WebSocket/TCP socket.
