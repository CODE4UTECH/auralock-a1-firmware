# AURALOCK A1 Firmware

**Cloud Native UID Scanner (Smart RFID Reader)**

AURALOCK A1 is an open-source firmware designed for developers to build their own RFID UID scanning hardware using an ESP32 and MFRC522 module.

The firmware enables ESP32 devices to scan RFID cards and connect directly to the AURALOCK cloud platform for device management, scan tracking, and card management.

This project is part of the AURALOCK platform ecosystem.

---

## Firmware Version

Current Stable Release

```
v1.2.2
```

Built using:

* ESP32 Arduino Framework
* MFRC522 RFID Driver
* ESP32 Preferences (NVS storage)

---

## Key Features

* Non-volatile configuration storage using ESP32 NVS
* Automatic WiFi reconnection (25 second timeout)
* Device heartbeat every 60 seconds
* 800 ms debounce protection to prevent duplicate scans
* Stable SPI communication (10 MHz) for reliable RFID reads
* LED scan indicator on GPIO 2

---

## Hardware Requirements

To build an AURALOCK A1 device you will need:

* ESP32 development board
* MFRC522 RFID reader module
* Jumper wires or custom PCB
* Optional LED indicator

---

## Arduino Firmware

The firmware source file is provided in this repository.

Developers can copy the `.ino` file into the Arduino IDE and upload it directly to their ESP32 device.

The firmware includes:

* WiFi management
* RFID scanning
* Portal communication
* Device heartbeat system

Configuration values are stored using non-volatile storage so the device does not require firmware modification after initial setup.

---

## Documentation

Complete documentation including wiring guides and setup instructions is available on the official AURALOCK website.

Hardware Wiring Guide
https://auralock.in/docs#a1-guide

Developer Documentation
https://auralock.in/developer

---

## Developer Portal

Devices can be registered and managed using the AURALOCK developer portal.

https://uid.auralock.in

The portal provides:

* Device dashboard
* Live scan monitoring
* Card UID manager
* Scan history logs
* USB setup tool

---

## Repository Structure

```
auralock-a1-firmware
│
├── firmware
│   └── auralock_a1.ino
│
├── README.md
├── LICENSE
└── CONTRIBUTING.md
```

---

## License

This project is released under the MIT License.

---

## Maintained By

AURALOCK Platform

Developed and maintained by
CODE4UTECH CONSULTANCY PVT. LTD.

Official Website
https://auralock.in

## AURALOCK Ecosystem

This project is part of the AURALOCK platform.

AURALOCK S1 – Smart access control for gyms, libraries, and membership spaces with cloud dashboard, mobile access, professional installation, and lifetime support.

https://auralock.in

AURALOCK S1 – Cloud-powered access control with membership automation for gyms, libraries, and managed facilities.