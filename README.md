# Matter-over-Thread 4-Channel Intelligent Relay Controller

![Robot_Front](https://github.com/jerryok826/ardunio_matter_thread_relay/blob/main/Images/relay_control_matter_thread.jpeg)

The board shown below is an Arduino Matter/Thread relay controller designed for use with an Apple HomeKit network. It uses an Arduino Nano Matter processor to control four onboard relays, which can be used to operate motors, lights, valves, or other electrical loads.

Each relay can be controlled locally using the onboard pushbuttons, allowing users to manually enable or disable individual outputs. Through the Apple Home app on an iPhone, each relay can also be monitored and controlled remotely from anywhere with internet access.

An integrated OLED display provides system status information, including Thread network signal strength. During initial setup, the display presents a QR code that can be scanned to quickly add the device to the Apple Home app.

The board also includes an I²C Qwiic connector for attaching external sensors and peripherals. For example, a rain sensor could be connected to automatically disable irrigation valves during rainfall, helping conserve water and prevent overwatering.

**Status:** The board completely supports relay control. The I²C Qwiic connector is wired wrong. There are other physical placements that need updating. So the board will be turned.

An industrial-grade, ultra-low-latency 4-Channel Relay Controller built using the **Arduino Nano Matter** (Silicon Labs MGM240S core). This project connects natively with Apple Home, Home Assistant, and other Matter ecosystems over a self-healing Thread mesh network.

Featuring an asynchronous hardware override architecture, non-blocking FreeRTOS thread watchdogs, and an integrated I2C OLED display that acts as a standalone **RF Survey & Walk-Testing Tool**, this project is designed for robust, long-term smart home panel automation.

---

## ✨ Key Features

- **Ecosystem Unified Build:** Native Matter protocol integration that auto-populates as four independent, sequential switch endpoints in your smart home application.
- **Zero App Dependence Override:** Local manual buttons run entirely on edge-triggered **Asynchronous Hardware Interrupts**. Relays switch locally within microseconds even if your smart home hub is offline or entirely destroyed.
- **Anti-Race Watchdog:** Implements a non-blocking 50ms throttled execution pacing engine to prevent Matter application layer data collisions and loop freezing.
- **Failsafe Boot Protection:** Firmware forces relay pins to an explicit `LOW` state prior to opening output drivers. Combined with physical pull-down networks, this eliminates clicks or accidental appliance cycles during microcontroller reboots.
- **Dual-OLED Matrix Engine:** Dynamic runtime support for both **SH1106** and **SSD1306** I2C displays via a simple preprocessor toggle.
- **Standalone RF Survey Tool:** Displays a dynamic pairing QR code on boot, then transitions automatically into a real-time system dashboard mapping your Thread Node Role and precise signal attenuation metrics (`dBm`) on a fast 3-second cycle.

---

## 🛠️ Hardware Requirements & Pinout

### Core Microcontroller
- [Arduino Nano Matter](https://arduino.cc) (Silicon Labs MGM240S)

### Supported Displays
- 0.96" I2C OLED Display (**SH1106 Recommended** for native border alignment, or SSD1306)

### Physical Netlist Connection Map


| Arduino Pin | Hardware Component | Electrical Specification / Circuit Design |
| :--- | :--- | :--- |
| **D2** | Relay 1 Trigger Output | Active-HIGH Latch (Must use 10kΩ external Pull-Down to GND) |
| **D3** | Relay 2 Trigger Output | Active-HIGH Latch (Must use 10kΩ external Pull-Down to GND) |
| **D4** | Relay 3 Trigger Output | Active-HIGH Latch (Must use 10kΩ external Pull-Down to GND) |
| **D5** | Relay 4 Trigger Output | Active-HIGH Latch (Must use 10kΩ external Pull-Down to GND) |
| **D6** | Manual Button 1 Input | Short-to-GND Momentary Switch (Relies on internal `INPUT_PULLUP`) |
| **D7** | Manual Button 2 Input | Short-to-GND Momentary Switch (Relies on internal `INPUT_PULLUP`) |
| **D8** | Manual Button 3 Input | Short-to-GND Momentary Switch (Relies on internal `INPUT_PULLUP`) |
| **D9** | Manual Button 4 Input | Short-to-GND Momentary Switch (Relies on internal `INPUT_PULLUP`) |
| **A4 / SDA**| OLED Serial Data Line | I2C Data (Include physical 4.7kΩ external Pull-Up to 3.3V) |
| **A5 / SCL**| OLED Serial Clock Line | I2C Clock (Include physical 4.7kΩ external Pull-Up to 3.3V) |
| **BTN_BUILTIN**| Factory Reset Trigger | Physical button on Nano Matter. Hold 10s to wipe Matter Fabric. |

> ⚠️ **CRITICAL DEVELOPER WARNING:** The Arduino Nano Matter GPIO pins safely output a maximum of **4mA to 8mA**. Standard mechanical relay coils draw between **70mA and 100mA**. **Do not wire relay coils directly to the Arduino pins.** You must use transistor switches (e.g., 2N2222) or optocouplers (e.g., PC817) on your custom PCB to isolate the driver currents.

---

## 💻 Firmware Installation & Configuration

### 1. Prerequisites
Ensure your Arduino IDE environment is properly updated:
1. Open the Arduino IDE, go to **Tools > Board > Boards Manager** and install the **Arduino Nano Matter** board package.
2. Go to **Tools > Manage Libraries** and install these exact dependencies:
   - `SSD1306Ascii` (by Bill Greiman)
   - `QRCode` (by Richard Moore)

### 2. IDE Compilation Configuration
Before flashing the firmware, you **must** configure the low-level stack settings:
- Go to **Tools > Protocol Stack** and select **Matter**. 
- *Note: If this is left on "None", the compiler will completely ignore all Matter initialization sequences.*

### 3. Display Selection Toggle
At the very top of the primary sketch file, uncomment the specific preprocessor definition that matches your physical OLED module hardware:

```cpp
// UNCOMMENT ONLY ONE of the options below to match your physical OLED panel:
#define DISPLAY_TYPE_SH1106      // Use this if your screen has an SH1106 controller
// #define DISPLAY_TYPE_SSD1306  // Use this if your screen has a true SSD1306 controller
```

---

## 📱 Provisioning & First-Time Setup

1. Flash the firmware to your board and open your serial monitor at **115200 baud**.
2. Upon boot, the board will automatically check its secure enclave storage rows. If it is uncommissioned, a custom **Version 3 QR Matrix** will automatically render on the left side of your OLED screen.
3. Open your native smart home app (e.g., **Apple Home**) on your iPhone.
4. Tap **Add Accessory** and point your camera at the OLED glass panel.
5. **Alternative Manual Flow:** If the camera cannot capture the display grid due to enclosure glare, tap *More Options / I Don't Have a Code*, select the module from the nearby Bluetooth list, and manually input the static 11-digit setup passcode:
   ```text
   Manual Setup Code: 3497-0259-332
   ```
6. Keep your phone close to the hardware for the first 30 seconds. The initial handshake executes over Bluetooth Low Energy (BLE) before moving the board permanently to your home's **Thread Mesh Network**.

---

## 📋 Custom PCB Manufacturing Failsafes

When translating this project layout into your CAD editor (KiCad/EasyEDA), include these high-utility design practices:
- **Forced Identity Shifts:** This firmware maps a customized `0xFFF2` Vendor ID and `0x8001` Product ID block to forcefully bypass the common iOS Bluetooth cache blacklisting traps that occur during early prototype drops.
- **Print the Code:** Because the 11-digit passcode (`3497-0259-332`) is mathematically derived from these static software IDs, it will never mutate. Drop a permanent text string reading `MATTER CODE: 3497-0259-332` directly onto your board's top and bottom **white silkscreen mask layers** as an operational backup.
- **Creepage Isolation slots:** Cut physical air slots completely through the FR4 PCB directly beneath your high-voltage relay trace paths to guarantee a safe creepage clearance gap between mains power lines and low-voltage logic traces.

