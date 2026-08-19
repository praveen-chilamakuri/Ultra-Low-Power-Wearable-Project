# **Overview**



This project implements an ultra‑low‑power wearable device capable of long‑term monitoring of:



Heart rate (ECG R‑peak detection)



Body temperature



Threshold‑based BLE alerts



The system integrates SHT31, AD8232, STM32F401RE, and ESP32 Wroom‑32, using a unified low‑power architecture designed to achieve 45-50 days of operation on a single AAA battery.



### **This repository contains:**



Embedded firmware (STM32 HAL + ESP32)



System‑level low‑power architecture



Power‑measurement methodology



Documentation and hardware photos



This work was completed as part of the MSc Embedded Systems Engineering dissertation at Coventry University (2026).

### 

### **Aim**



To design and evaluate an ultra‑low‑power wearable capable of multi‑week physiological monitoring using optimised sensing, processing, and communication strategies.



### **Hardware Used**



STM32F401RE Nucleo (Stop/Sleep modes, DMA sampling)



ESP32 Wroom‑32 DevKit (Deep sleep + BLE alerts)



AD8232 ECG Front-End



SHT31 Temperature Sensor



AAA Battery (Projected 45-50 days)



Multimeter (IDD pin measurements)



# 

# **System Architecture**



### **Key Low-Power Techniques**



Duty‑cycled sensing (7‑second sampling window)



STOP‑mode scheduling (STM32)



DMA‑based ECG sampling (125 Hz, 250‑sample batches)



Sensor‑level power gating (SHT31, AD8232 SDN pin)



Event‑driven BLE alerts (temperature, BPM > 102, leads‑off)



Peripheral de‑initialisation + clock gating





# **Experimental Results**



#### **Parameter                      Result**



Average Current Consumption       ------->          	0.68 mA



Battery Life Projection (AAA)     ------->            	45-50 days



Temperature Accuracy              ------->              Stable single shot SHT31 readings



ECG Performance                   ------->            	Reliable R peak detection



BLE Alerts                        ------->              Triggered on threshold events only





# **Repository Structure**



### **Ultra-Low-Power-Wearable-Project**/

│

├── Core/                 ---->    # STM32 HAL core files

|

├── Drivers/              ---->    # STM32 peripheral drivers

|

├── ProjectFiles/         ---->    # CubeMX + CubeIDE project files

|

├── ESP32/                ---->    # ESP32 BLE + deep sleep firmware

|

├── Docs/                 ---->    # Documentation, diagrams, photos

|

├── README.md             ---->    # Project overview

|

├── LICENSE               ---->    # MIT License

|

└── .gitignore





# **How to Build \& Flash**



### **STM32**



Open project in STM32CubeIDE



Build → Flash via ST‑Link



Power via external supply for accurate IDD measurements



### **ESP32**



Open in Arduino IDE / PlatformIO



Flash via USB‑UART



BLE alerts enabled by default



### **BLE Alerts Implemented**



Temperature > 38°C



BPM > 102 bpm



ECG leads‑off detection



Alerts are event‑driven, not continuous, reducing wireless overhead.



## 

# **Future Work**

# 

Transition to STM32WB06 (integrated BLE, lower IDD)



Custom PCB to eliminate dev‑board leakage



Improved ECG filtering pipeline

## 

## 

# **License**



**This project is licensed under the MIT License.**

