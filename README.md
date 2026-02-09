| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |
# ESP32 Modular Framework: BLE | WIFI | MQTT | NVS | GPIO
This repository contains a professional-grade, modular firmware boilerplate for the ESP32. It is designed with a strict separation of concerns, allowing for easy testing and maintenance of individual hardware and protocol components.

## 🏗 Modular Architecture
The project is structured into **Application Service Wrappers (ASW)**. Each module manages its own state, tasks, and event handlers.

### Component Breakdown:
* **BLE Module (NimBLE):** Handles GAP/GATT, security (Passkeys/Bonding), and Wi-Fi provisioning.
* **Wi-Fi Module:** Manages Station mode, auto-reconnection, and credential handling via an internal command queue.
* **MQTT Module:** Connects to public brokers (HiveMQ) to facilitate remote command/control.
* **NVS Module:** Manages persistent storage for Wi-Fi credentials and system calibration data.
* **GPIO Module:** Simple abstraction for hardware interaction (LEDs/Sensors).

### 1. Set the Target
Before building, set the correct chip target:
```bash
idf.py set-target <chip_name>
```

### 2. Configure the Project
Open the configuration menu:
```bash
idf.py menuconfig
```
Bluetooth: Navigate to Component config -> Bluetooth and enable the NimBLE stack.
Example Configuration: * Select I/O capabilities (e.g., Display_YesNo or Just_works).
Toggle Bonding, MITM, and Secure Connection (SC).

3. Build and Flash
```bash
idf.py -p PORT flash monitor
```

📂 File Explanations & Usages
🟢 main.c (The Orchestrator)
Initializes all modules in a specific order to ensure dependencies (like NVS and GPIO) are ready before the network stacks start.

🔵 ASW/ble/ (BLE & Provisioning)
Uses the NimBLE stack for better memory efficiency.
Provisioning: Write to the Wi-Fi Provision Characteristic (0x2F02) with the format SSID:myssid;PASS:mypass.
Security: Supports Passkey injection and Numeric Comparison for secure bonding.

🟡 ASW/wifi/ (Connection Management)
Handles the Wi-Fi state machine.
It automatically attempts to load credentials from NVS.
Once an IP is obtained, it automatically signals the MQTT module to start.

🔴 ASW/mqtt/ (Cloud Communication)
Connects to mqtt://broker.hivemq.com.
Subscribe: Listens to tahirshaikh/esp32/cmd.
Commands: Supports LED_ON, LED_OFF, and LED_TOGGLE.

🟣 ASW/nvs/ & ASW/gpio/
NVS: Reliable helper functions to save/load Wi-Fi credentials to flash memory.
GPIO: Controls the on-board LED (GPIO 2) with simple high-level functions like gpio_app_toggle().

🛠 Operation Flow
Init Phase: Hardware and Storage are initialized.
Discovery Phase: BLE starts advertising "Smart Home" services.
Connection Phase: Wi-Fi connects (via stored creds or BLE provisioning).
Action Phase: MQTT connects and the device becomes controllable via the cloud.

👨‍💻 Developer
Tahir Shaikh Firmware Embedded Engineer
