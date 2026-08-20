# Ultra-Low-Power Wearable for Long-Term Physiological Monitoring

**License:** `MIT` | **MCU:** `STM32F401RE` | **Wireless:** `ESP32` | **Field:** `Embedded Systems` | **Sensors:** `SHT31, AD8232` | **Focus:** `Low-Power`

An ultra‑low‑power wearable device designed for long‑term monitoring of **Heart Rate (ECG R‑peak detection)** and **Body Temperature**. The architecture integrates specialized duty-cycling, DMA data transfers, and sensor-level power gating to deliver continuous multi-week monitoring from a single AAA battery.

This project was developed as part of the **MSc Embedded Systems Engineering dissertation at Coventry University (2026)**.

---

## ⚡ Technical Highlights
* 🔋 **0.68 mA average current** measured via IDD pins
* ⏳ **45-50 days projected battery life** on a single AAA cell
* 🔄 **DMA‑based ECG sampling** at 125 Hz (250‑sample batches)
* 📡 **Event‑driven BLE alerts** (ESP32 deep sleep except during anomalies)
* 🔌 **Full sensor power gating** for SHT31 & AD8232
* ⚙️ **Deterministic low‑power state machine** controlling all sensing and radio activity
* ⏱️ **STOP‑mode scheduling** with RTC wakeups every 7 seconds
* ⚠️ **Hardware‑level leads‑off detection** using AD8232 LOD pins

---

## 📌 Aim & Features
To design and evaluate an ultra‑low‑power physiological monitor utilizing optimized sensing, processing, and communication strategies.
* **Unified Low-Power Scheduling:** Orchestrated sleep profiles across all hardware nodes.
* **Event-Driven Communications:** Wireless transmissions run exclusively during active alert exceptions.
* **Hardware Isolation:** Dynamic power gating blocks static leakage currents from inactive peripherals.

---

## 🛠️ Hardware Architecture & Pin Connections

The system bridges an **STM32F401RE Nucleo** (Main MCU managing scheduling, sampling, and processing) and an **ESP32 Wroom‑32 DevKit** (Secondary network MCU handling BLE alerts).

![Architecture Flow Diagram](./Docs/architecture & execution/architecture flow.svg)


| STM32 Pin | Label / Function | Connected To | Purpose |
| :--- | :--- | :--- | :--- |
| **PA0** | `SHT31_PWR` | SHT31 VCC | Power gating the temperature sensor |
| **PA1** | `ADC1_IN1` | AD8232 OUTPUT | Analog ECG signal sampling |
| **PA2** | `USART2_TX` | Debug Link / Host PC | Serial telemetry and system debugging |
| **PA3** | `USART2_RX` | Debug Link / Host PC | Serial telemetry and system debugging |
| **PA4** | `AD8232_SDN` | AD8232 SDN | Power gating / shutting down ECG front-end |
| **PA5** | `LO_PLUS` | AD8232 LOD+ | Leads-off detection (Positive) |
| **PA6** | `LO_MINUS` | AD8232 LOD- | Leads-off detection (Negative) |
| **PA7** | `ESP32_WKUP` | ESP32 Ext Wakeup (GPIO4 / RTC_GPIO10) | Waking up ESP32 from Deep Sleep via GPIO pulse |
| **PB6** | `I2C1_SCL` | SHT31 SCL | I2C Clock for temperature sensor |
| **PB7** | `I2C1_SDA` | SHT31 SDA | I2C Data for temperature sensor |

### Key Low-Power Techniques
* ⏱️ **Duty‑Cycled Sensing:** Tightly managed 7‑second system-wide execution window.
* 💤 **STOP‑Mode Scheduling:** STM32 rests in low-power STOP mode between measurement blocks.
* 📊 **DMA‑Based ECG Sampling:** Background ADC sampling at 125 Hz in 250‑sample batches.
* 🔌 **Sensor Power Gating:** Physical power isolation of SHT31 and AD8232 when idle.
* 🛑 **Clock Gating:** Active peripheral de‑initialization before dropping to low-power states.

---

## 📊 Experimental Results & Power Analytics

| Parameter | Result | Operational Notes |
| :--- | :--- | :--- |
| **Average Current Consumption** | 0.68 mA | Measured via IDD jumper pins |
| **Battery Life Projection** | ~45 Days | Adjusted estimate derived from continuous deployment profile |
| **Temperature Accuracy** | Stable | Verified through single-shot SHT31 sensor polling |
| **ECG Performance** | Reliable | Clean R-peak extraction under static test metrics |
| **BLE Alerts** | Event-Driven | Triggered exclusively during anomalous metric thresholds |

> 🔋 **Battery Lifespan Calculation:**
> * **Operating Hours:** 800 mAh / 0.68 mA ≈ 1,176.47 Hours (≈ 49 Days Theoretical)
> * **Real-World Estimation:** Adjusted to **45 days** to account for battery self-discharge, voltage dropoff, and peak BLE transmission spikes.

---

## 🔄 System Execution Flow

The system moves through a deterministic sequence to optimize the energy budget during every 7-second epoch:

```text
       ┌─────────────────────────────────────────────────────────┐
       │   [RTC Timer] Wakes up STM32 from STOP Mode (Every 7s)  │
       └───────────────────────────┬─────────────────────────────┘
                                   │
                                   ▼
       ┌─────────────────────────────────────────────────────────┐
       │ 1. [SHT31 Temperature Cycle]                            │
       │    - Power-gate ON SHT31 & AD8232 via GPIO              │
       │    - Transmit single-shot command; STM32 enters Sleep   │
       │    - Wake up, read I2C data, process alert threshold    │
       │    - Power-gate OFF SHT31 sensor completely             │
       └───────────────────────────┬─────────────────────────────┘
                                   │
                                   ▼
       ┌─────────────────────────────────────────────────────────┐
       │ 2. [AD8232 ECG Cycle]                                   │
       │    - Sleep during mandatory 150ms AD8232 settling phase │
       │    - Evaluate Leads-Off pins (Bypass if disconnected)   │
       │    - Stream 2s of ADC data at 125 Hz directly via DMA   │
       │    - DMA full wakes STM32 -> Power-gate OFF AD8232      │
       │    - Extract R-peaks and compute BPM                    │
       └───────────────────────────┬─────────────────────────────┘
                                   │
                                   ▼
       ┌─────────────────────────────────────────────────────────┐
       │ 3. [Alert Evaluation & Radio Coordination]               │
       │    - Threshold Breached: Pulse ESP32_WKUP -> Transmit   │
       │      BLE payload (1.5s window) -> Re-enter Deep Sleep   │
       │    - Normal State: ESP32 remains locked in Deep Sleep   │
       │    - De-initialize peripherals -> Re-enter STOP Mode     │
       └─────────────────────────────────────────────────────────┘
```

### 🚨 BLE Alerts Implemented
Wireless operations run strictly on an event-driven basis to mitigate overhead:
* **Hyperthermia:** Temperature > 38°C
* **Tachycardia:** Heart Rate > 102 bpm
* **Hardware Fault:** ECG electrode separation (`LOD+` / `LOD-` hardware pins)

---

## 📂 Repository Structure

```text
Ultra-Low-Power-Wearable-Project/
├── Core/             # STM32 HAL core application source & include files
├── Drivers/          # STM32 peripheral driver configuration files
├── ProjectFiles/     # CubeMX (.ioc) + CubeIDE project files
├── ESP32/            # ESP32 BLE communications and deep sleep firmware
├── Docs/             # Technical documentation, schematics, and photos
├── LICENSE           # MIT License
└── README.md         # Project configuration description
```

---

## 🚀 How to Build & Flash

### 1. STM32 Firmware (Main MCU)
1. Launch **STM32CubeMX** and load the `.ioc` configuration file inside `ProjectFiles/`.
2. Generate code targeting the **STM32CubeIDE** toolchain.
3. Open the workspace in STM32CubeIDE and replace the placeholder `main.c` file with the low-power optimized source located in `Core/Src/`.
4. Compile the source and flash the MCU utilizing an **ST-Link v2** debug interface.

### 2. ESP32 Firmware (Radio Node)
1. Initialize the project directory using **Arduino IDE** or **PlatformIO** targeting an ESP32 DevKit module.
2. Load the source code located within `ESP32/esp32_main.c`.
3. Verify that native BLE framework libraries are correctly integrated inside the compilation path.
4. Flash the target binary image to the hardware using a standard **USB-to-UART** serial interface bridge.

---

## 🔮 Future Development Path
* **Silicon Integration:** Migrate design to the **STM32WB06** system-on-chip to combine processing and BLE operations onto a single die, eliminating inter-MCU link overhead.
* **Form-Factor Engineering:** Layout a dedicated, multi-layer **Custom PCB** to deprecate developer kit modules and remove parasitic LDO and UART bridge leakages.
* **Algorithmic Hardening:** Move digital ECG filtering into a localized streaming pipeline using circular DMA buffers to drastically reduce RAM allocation and latency.
* **RTOS Integration:** Implement an RTOS framework to establish modular task scheduling, isolating time-critical biosensing threads from communication stacks while optimizing low-power sleep states.

---

## 📄 License
This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.
