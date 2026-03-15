# BL602 PQC Sender Firmware

## Table of Contents
- Overview
- Features
- Project Structure
- Requirements
- Configuration
- Building the Firmware
- Flashing the Board 
- Running the System
- Runtime Flow
- Device Log Format
- CoAP Endpoints
- Troubleshooting
- Security Notes


## Overview
This project contains the **sender-side firmware** for a post-quantum secure communication demo on the **Bouffalo Lab BL602 / PineCone** platform.

The sender:
- connects to Wi-Fi
- broadcasts a CoAP request to discover the gateway
- fetches the gateway **ML-KEM-512** public key
- performs **ML-KEM encapsulation**
- derives an **AES-128** session key using **HKDF-SHA-256**
- encrypts a fixed plaintext using **AES-CCM**
- sends one protected CoAP message to the gateway
- blinks an LED on success



## Features
- BL602 Wi-Fi station mode
- CoAP over UDP on port **5683**
- Broadcast-based gateway discovery
- **ML-KEM-512** key encapsulation
- **HKDF-SHA-256** session key derivation
- **AES-128-CCM** authenticated encryption
- Minimal CoAP implementation for embedded use
- Serial log output for debugging
- GPIO5 LED indication after send

## Project Structure
```text
sender/
├── Makefile
├── genromap
├── proj_config.mk
├── sender/
│   ├── bouffalo.mk
│   ├── main.cpp
│   ├── wifi.cpp
│   ├── coap_minimal.c
│   ├── pqkem_kem.c
│   ├── pqkem_kem.h
│   ├── include/
│   │   ├── coap_minimal.h
│   │   ├── pqkem_kem.h
│   │   └── wifi.h
│   └── kyber/
│       └── kyber512_ref/
└── tools/
    ├── main.cpp
    └── monitor/
        └── monitor.py
```

## Requirements

### Hardware
- 1 × **PineCone / BL602** board for the sender
- 1 × **BL602 gateway board** running the matching receiver firmware
- 1 × Wi-Fi access point / router
- Optional LED on **GPIO5** with resistor
- USB cable for flashing and serial logs

### Software
- BL60X / BL602 SDK toolchain installed
- Python virtual environment activated
- `make`
- `blflash`
- Serial terminal such as `screen`

## Configuration

### Wi-Fi credentials
Edit:

```c
sender/sender/include/wifi.h
```

Set your SSID and password:

```c
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PW   "YOUR_WIFI_PASSWORD"
```

### SDK path
Set the SDK path before building:

```bash
export BL60X_SDK_PATH=/path/to/bl_iot_sdk
```

## Building the Firmware
The project includes a helper script named `genromap`. Build from the project root:

```bash
cd sender
./genromap
```

This starts the compilation process for the BL602 target.

## Flashing the Board
The flashing steps below follow the provided **Compiling and Flashing** guide.

### 1. Put the PineCone into flashing mode
- Bridge **IO8** with **H**
- Press the **reset** button

### 2. Flash the compiled firmware
After a successful build, flash the generated binary from `build_out/`.

#### GNU/Linux
```bash
blflash flash build_out/sender.bin --port /dev/ttyUSB0
```

If your BL602 appears on another serial device, replace `/dev/ttyUSB0` with the correct port.

#### Windows
```bash
blflash flash build_out\sender.bin --port COM5
```

Replace `COM5` with the actual port shown in Device Manager.

### 3. Return the PineCone to normal operating mode
- Bridge **IO8** with **L**

### 4. Open the serial console (optional but recommended)
#### GNU/Linux
```bash
screen /dev/ttyUSB0 2000000
```

#### Windows
Use a serial terminal and set the baud rate to:

```text
2000000
```

### 5. Reset the board
Press the **reset** button again to start the sender firmware.

## Running the System

### Terminal 1 – Gateway
Flash and start the matching **gateway firmware** on a second BL602 board.

### Terminal 2 – Sender
Open the sender serial console and reset the sender board.

### Expected run sequence
1. Sender connects to Wi-Fi
2. Sender broadcasts CoAP discovery on port **5683**
3. Gateway replies with its **ML-KEM-512 public key**
4. Sender performs encapsulation and derives the AES key
5. Sender encrypts the plaintext using AES-CCM
6. Sender sends a protected `/pqkem-data` message
7. Gateway decrypts and displays the plaintext

## Runtime Flow
The sender implementation in `sender/sender/main.cpp` performs this sequence:

1. Initialize platform, Wi-Fi stack, and LED GPIO
2. Wait until Wi-Fi and DHCP are ready
3. Open a UDP socket with broadcast enabled
4. Send a CoAP request to **`/pqkem-pk`** using broadcast
5. Accept the first valid response carrying the gateway public key
6. Run **ML-KEM-512 encapsulation**
7. Derive the AES key with **HKDF-SHA-256**
8. Encrypt the message **"Hello, message from sender"** using **AES-CCM**
9. Send a CoAP message to **`/pqkem-data`**
10. Blink the LED after successful transmission

## Device Log Format
Typical serial log output:

```text
=== Sender: Post-quantum key exchange (ML-KEM-512, level 1) ===
[starter] Waiting for Wi-Fi to be ready...
[wifi] Connected to "<SSID>"!
[wifi] IP  : <ip>
[wifi] GW  : <gateway>
[starter] Wi-Fi ready, starting SENDER task
[sender] Task started (ML-KEM-512, level 1)
[sender] Broadcasting Discovery Packet to 255.255.255.255...
[sender] RESPONSE RECEIVED from IP: <gateway_ip>
[sender] pqkem_encapsulate done in <ms> ms
[sender] KEM + HKDF done, AEAD key ready
[sender] AEAD encrypt done in <ms> ms
[sender] Sent ONE protected message
```

## CoAP Endpoints

### Public key request
- **Method:** `POST`
- **Path:** `/pqkem-pk`
- **Port:** `5683`
- **Destination:** broadcast first, then gateway IP
- **Purpose:** fetch the gateway ML-KEM public key

### Protected data message
- **Method:** `POST`
- **Path:** `/pqkem-data`
- **Port:** `5683`
- **Purpose:** send the KEM ciphertext and AEAD-protected payload

## Troubleshooting

### Build fails
- Check that `BL60X_SDK_PATH` is set
- Activate the correct Python virtual environment
- Verify the BL602 toolchain installation

### Flash fails
- Confirm the board is in **flashing mode** using **IO8 ↔ H**
- Check the correct serial port
- Make sure no other terminal is already using the port

### No Wi-Fi connection
- Recheck `wifi.h`
- Ensure the access point is reachable
- Confirm SSID and password are correct

### No gateway response
- Make sure the gateway firmware is running
- Ensure both boards are on the same network
- Confirm UDP port **5683** is not blocked

## Security Notes
- The sender does not send plaintext over the network
- The AES key is derived from the ML-KEM shared secret using HKDF
- The protected payload uses **AES-CCM** for confidentiality and integrity
- Network headers remain visible even though the message payload is encrypted



