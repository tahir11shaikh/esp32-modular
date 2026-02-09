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
Orchestrates the modular boot sequence.
Initializes all modules in a specific order to ensure dependencies (like NVS and GPIO) are ready before the network stacks start.
```bash
void app_main(void)
{
    nvs_app_init();   /* Persistent Storage */
    gpio_app_init();  /* Hardware IO */
    ble_app_init();   /* Bluetooth Stack */
    wifi_app_init();  /* Wi-Fi Stack */
    mqtt_app_init();  /* Messaging Protocol */
}
```

🔵 ASW/ble/ (BLE & Provisioning): 
Handles the NimBLE host task, GAP/GATT configuration, and security pairing.
Provisioning: Write to the Wi-Fi Provision Characteristic (0x2F02) with the format SSID:myssid;PASS:mypass.
Security: Supports Passkey injection and Numeric Comparison for secure bonding.
```bash
/* BLE Module Summary:
 * - Passkey/Numeric Comparison pairing
 * - Custom GATT Services for Smart Home control
 * - BLE Provisioning for Wi-Fi credentials
 */
// ... (Refer to repository for full implementation)
```

🟡 ASW/wifi/ (Connection Management)
Handles the Wi-Fi state machine.
Manages connection states and triggers MQTT on successful IP acquisition.
It automatically attempts to load credentials from NVS.
Once an IP is obtained, it automatically signals the MQTT module to start.
```bash
/* Wi-Fi Module Summary:
 * - Station Mode (STA)
 * - Auto-reconnect logic
 * - Credential recovery from NVS
 */
```

🔴 ASW/mqtt/ (Cloud Communication)
Connects to mqtt://broker.hivemq.com.
Subscribe: Listens to tahirshaikh/esp32/cmd.
Commands: Supports LED_ON, LED_OFF, and LED_TOGGLE.
Integrates with HiveMQ to allow remote control of the ESP32.
```bash
/* MQTT Module Summary:
 * - Default Broker: broker.hivemq.com
 * - Command Topic: tahirshaikh/esp32/cmd
 * - Supported Commands: LED_ON, LED_OFF, LED_TOGGLE
 */
```

🟣 ASW/nvs/
Handles non-volatile storage for Wi-Fi credentials and PHY calibration.
NVS: Reliable helper functions to save/load Wi-Fi credentials to flash memory.

🟡 ASW/gpio/
GPIO: Controls the on-board LED (GPIO 2) with simple high-level functions like gpio_app_toggle().

🛠 Functionality & Flow
Boot: The device loads stored Wi-Fi credentials from NVS.
Provisioning: If no credentials exist, the user can write to the BLE characteristic UUID_WIFI_PROV (0x2F02) with format: SSID:name;PASS:password.
Cloud Sync: Once Wi-Fi is connected, the MQTT client starts and subscribes to the command topic.
Remote Control: The user sends "LED_ON" via MQTT, which triggers the GPIO module to turn on the physical LED.

👨‍💻 Developer
Tahir Shaikh Embedded Architect (10+ Years Experience)
