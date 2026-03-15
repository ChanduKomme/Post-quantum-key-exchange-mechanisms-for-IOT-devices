# Post-Quantum Key Exchange for IoT Devices 🔐

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-Bouffalo_Lab_BL602-blue)](https://www.bouffalolab.com/)
[![Crypto](https://img.shields.io/badge/Crypto-ML--KEM--512_%2B_AES--CCM-green)](https://csrc.nist.gov/projects/post-quantum-cryptography)
[![PineCone](https://img.shields.io/badge/PineCone-Wiki-blue)](https://wiki.pine64.org/wiki/PineCone)
[![SDK](https://img.shields.io/badge/SDK-Info-red)](https://pine64.github.io/bl602-docs/)

## 🔗 Project Overview

The **Post-Quantum Key Exchange Mechanisms for the IoT** project was developed at **Schmalkalden University of Applied Sciences**. It demonstrates a complete post-quantum secure communication prototype on **PineCone / BL602** microcontrollers using **ML-KEM-512**, **HKDF-SHA-256**, and **AES-128-CCM** over **CoAP/UDP** on a local Wi-Fi network.

> [!NOTE]
> The system uses three BL602 boards: a **Sender**, a **Gateway/Receiver**, and a **Sniffer**. The Sender discovers the Gateway, retrieves its ML-KEM public key, establishes a shared secret, derives an AES key, and sends a protected CoAP message.

> [!TIP]
> The repository is modular. The sender, receiver, and sniffer are kept as separate firmware projects, and the architecture, prototype images, attack captures, short paper, and final report are included in the same repository.

> [!IMPORTANT]
> This project is motivated by the **"Harvest Now, Decrypt Later"** threat. It shows how long-lived IoT traffic can be protected today with post-quantum key establishment on constrained embedded hardware.

---

## Abstract

Classical asymmetric cryptography used in many IoT devices may become vulnerable once large-scale quantum computers are available. This project presents a proof-of-concept post-quantum secure communication system for resource-constrained IoT devices based on **PineCone / BL602** boards. The prototype uses **ML-KEM-512** to establish a shared secret, **HKDF-SHA-256** to derive a session-specific AES key, and **AES-128-CCM** to protect application data carried over **CoAP/UDP**. The result is a practical demonstration that post-quantum secure communication can be integrated into small embedded systems with manageable latency and memory usage.

## System Scenario

The system implements a layered secure IoT communication model using three BL602-based nodes.

- The **Sender** connects to Wi-Fi, discovers the Gateway by CoAP broadcast, requests the Gateway public key, performs **ML-KEM-512 encapsulation**, derives an AES key using **HKDF-SHA-256**, encrypts the application payload using **AES-CCM**, and sends the protected message over **CoAP/UDP**.
- The **Gateway/Receiver** stores a long-term **ML-KEM-512** key pair in internal flash memory, responds to the public-key request, receives the protected data packet, performs **ML-KEM-512 decapsulation**, derives the same AES key, authenticates and decrypts the payload, checks replay freshness using a nonce cache, and displays the plaintext on an **SSD1306 OLED**.
- The **Sniffer** runs in Wi-Fi monitor mode, forwards captured frames over UART to a host-side tool, and allows real-time observation in **Wireshark** without revealing the plaintext payload.

This design combines post-quantum key establishment, key derivation, authenticated encryption, monitoring, and runtime validation in a compact IoT prototype.

![Architecture Diagram](Architecture/Architecture%20diagram.jpg)

## Protocol Flow

1. Sender and Gateway boot and connect to the same Wi-Fi network.
2. Gateway loads or generates its long-term **ML-KEM-512** key pair.
3. Sender requests the Gateway public key using **CoAP/UDP**.
4. Sender performs **ML-KEM-512 encapsulation**, producing:
   - a ciphertext
   - a shared secret
5. Both sides derive the same **AES-128 session key** using **HKDF-SHA-256**.
6. Sender encrypts the application payload using **AES-CCM**.
7. The protected CoAP message is sent to the Gateway.
8. Gateway verifies and decrypts the payload.
9. On success, the plaintext is shown on the OLED and status LEDs indicate completion.

![Protocol Flow](Architecture/sequence%20diagram.jpg)

## What This Project Does

This repository contains the complete prototype.

### Sender
1. Connects to the local Wi-Fi network.
2. Broadcasts a CoAP request to discover the Gateway.
3. Requests the Gateway public key via `/pqkem-pk`.
4. Performs **ML-KEM-512 encapsulation** to create a ciphertext and shared secret.
5. Derives an **AES-128** key using **HKDF-SHA-256**.
6. Encrypts the plaintext using **AES-CCM**.
7. Sends the protected payload to the Gateway using `/pqkem-data`.
8. Blinks a status LED on successful transmission.

### Gateway / Receiver
1. Connects to the local Wi-Fi network.
2. Loads an existing **ML-KEM-512** key pair from flash, or generates and stores one.
3. Listens on **UDP port 5683** for CoAP messages.
4. Responds to `/pqkem-pk` with its public key.
5. Receives a protected `/pqkem-data` message.
6. Performs **ML-KEM-512 decapsulation** to recover the shared secret.
7. Re-derives the AES key using **HKDF-SHA-256**.
8. Authenticates and decrypts the payload using **AES-CCM**.
9. Displays the plaintext on the **SSD1306 OLED**.
10. Detects replay and authentication failure events and reports them using LED, OLED, and serial logs.

### Sniffer
- Captures nearby IEEE 802.11 traffic in monitor mode.
- Streams the captured frames over serial to the host.
- Works with the included Python monitor tool to feed packet data into **Wireshark**.

***System-output***
![Prototype](Prototype/System_Output/system_output.jpg)

***Data packets captured in monitor mode***
![Prototype](Prototype/System_Output/Data_packets(monitor_mode).jpg)

---
## Prototype

![Prototype](Prototype/Prototype.jpg)

## 🎥 Demonstration

A demo file is included at:

[Watch full demo of project with MIMT and tamper attacks](Prototype/demo.mp4)

> [!NOTE]
> The repository also contains screenshots of passive sniffing and tampering experiments under `Prototype/Attacks/`.

## Crypto Explanation

The system uses a simple and clean **KEM → KDF → AEAD** design.

### Post-Quantum Key Establishment (ML-KEM)
- The system uses **ML-KEM-512**, the smallest parameter set of the ML-KEM family.
- The Gateway stores a long-term public/private key pair in flash memory.
- The Sender uses the Gateway public key to perform encapsulation and generate:
  - an **ML-KEM ciphertext**
  - a **32-byte shared secret**

### Key Derivation (HKDF-SHA-256)
- The shared secret is passed through **HKDF-SHA-256**.
- A **128-bit AES session key** is derived.
- This prevents direct use of the raw ML-KEM shared secret.

### Authenticated Encryption (AES-128-CCM)
- Application data is protected using **AES-128-CCM**.
- This provides both:
  - **confidentiality**
  - **integrity**
- The message includes a nonce and authentication tag.
- If ciphertext, nonce, or tag is modified, the Gateway rejects the packet.

## Security Analysis

The project demonstrates protection against the main network-level risks considered in the report and code:

- **Quantum resistance:** the key-establishment step uses **ML-KEM-512** instead of classical RSA/ECC.
- **Confidentiality:** payload data is encrypted using **AES-CCM**.
- **Integrity:** modified ciphertext is rejected during authentication.
- **Replay handling:** the Gateway keeps a nonce cache and rejects reused values.
- **Traffic validation:** the Sniffer and Wireshark allow packet observation while the protected payload remains unreadable.

A network attacker can still observe metadata such as IP/UDP/CoAP headers, message timing, and packet sizes, but cannot recover the plaintext without the shared secret.

## Threat Model

The attacker is assumed to be on the same **Wi-Fi / LAN** as the Sender and Gateway. The attacker can:

- observe traffic
- inspect IP and UDP headers
- view CoAP requests and responses
- attempt passive sniffing
- attempt packet tampering
- position themselves in the communication path

The attacker does **not** have access to the Gateway private ML-KEM key stored on the device.

---

## Security Assumptions

- The **Gateway** is trusted to store the long-term private key securely in flash.
- The physical device is assumed to remain trustworthy.
- **CoAP over UDP** is used as the transport layer.
- The prototype relies on cryptographic protection rather than trust in the local network.

## Performance Measurements

Performance values below come from the included report and match the implementation in this repository.

| Parameter | Result | Meaning |
|---|---:|---|
| ML-KEM-512 encapsulation time | ~11–12 ms | Time taken by sender to generate ciphertext and shared secret |
| ML-KEM-512 decapsulation time | ~11–12 ms | Time taken by gateway to recover the shared secret |
| AES-CCM encryption time | < 1 ms | Time taken to encrypt the message |
| AES-CCM decryption time | < 1 ms | Time taken to decrypt the message |
| Public key size | 800 bytes | Size of gateway public key |
| Ciphertext size | 768 bytes | Size of ML-KEM ciphertext |
| Shared secret size | 32 bytes | Secret used to derive AES key |

### Gateway Memory Usage

| Parameter | Result | Meaning |
|---|---:|---|
| Flash-backed firmware size | 615,598 bytes | Program code and read-only data |
| Final binary size | 615,724 bytes | Generated gateway firmware file |
| Static RAM-backed memory | 109,092 bytes | Fixed RAM reserved by the gateway build |
| UDP receive buffer | 1,024 bytes | Buffer used to receive CoAP packets |
| Key storage structure | 2,440 bytes | Space used to store gateway key material |

The post-quantum handshake is the main cryptographic cost, while AES-CCM adds very little delay for the short demonstration message.

## Security Evaluation & Attack Results

This repository includes assets from controlled lab testing of the system.

### Passive Sniffing Attack

**Objective**
- Determine whether an attacker can read the transmitted message.

**Method**
- ARP spoofing was used to observe traffic.
- Traffic was captured in Wireshark.
- Packet structure and payload bytes were inspected.

**Arp-Spoffing**
---
![ARP Spoofing](Prototype/Attacks/Arp%20spoofing/Arp%20spoffing.jpeg)

**Arp-table**
---

![ARP Table](Prototype/Attacks/Arp%20spoofing/ARP%20table.jpeg)

**Captured packets(Attacker)**
---

![Captured Packets](Prototype/Attacks/Arp%20spoofing/data%20packets.jpg)

**Payload**
---

![Encrypted Payload](Prototype/Attacks/Arp%20spoofing/Payload.jpg)

**Result**
- The attacker can see CoAP packets, IP/UDP headers, ports, timing, and sizes.
- The attacker cannot recover the plaintext.
- The payload appears as encrypted / random-looking bytes.

---

### Tampering Attack (Bit Flip Injection)

**Objective**
- Check whether modification of encrypted packets is detected.

**Method**
- The protected payload was modified after encryption.
- The altered packet was forwarded to the Gateway.

**Tamper-attack**
---

![Tamper Attack](Prototype/Attacks/Tampering/tamper%20attack.jpg.jpeg)


**Tamper-result**
---

![Tamper Result](Prototype/Attacks/Tampering/Tamper%20attack%20result.jpg.jpeg)

**Conclusion**
- AES-CCM authentication detects the modification.
- The Gateway rejects the modified message and reports authentication failure.

---

## Security Properties Verified

| Attack Type / Property | Status | Mechanism |
|---|---|---|
| Packet sniffing | Yes | ML-KEM + AES-CCM protect the payload |
| Packet tampering | Yes | AES-CCM authentication tag |
| Traffic observability without plaintext disclosure | Yes | Sniffer + Wireshark |
| Public-key authentication during retrieval | Not yet fully implemented | Future work |

## Why This System Is Secure

1. **ML-KEM-512** provides post-quantum key establishment.
2. **HKDF-SHA-256** derives an application-specific AES key.
3. **AES-128-CCM** provides confidentiality and integrity.
4. The Gateway performs nonce-based replay checks.
5. Any ciphertext or tag modification causes authentication failure.

> [!NOTE]
> Passive sniffing or ARP spoofing does not break the cryptography by itself. An attacker can observe traffic, but cannot decrypt the protected payload without the shared secret and the Gateway private key.

## Security Features

### Post-Quantum Key Exchange
- Sender uses the Gateway public key to encapsulate and generate:
  - `ciphertext`
  - `shared_secret`
- Gateway decapsulates using its private key and recovers the same `shared_secret`.

### Key Derivation
Both sides run:

```text
HKDF(shared_secret, info="ML-KEM-AEAD") → AES-128 key
```

### Authenticated Encryption
AES-CCM provides:
- confidentiality
- integrity

If ciphertext, nonce, or tag is changed, the Gateway reports **AUTH FAIL**.

### Runtime Monitoring
- Sender LED indicates successful send.
- Gateway LED indicates success or failure.
- OLED shows the decrypted plaintext or failure state.
- Sniffer + monitor tool + Wireshark show what happens on the network.

---

## Hardware Requirements

### Mandatory
- **1 × PineCone BL602** as Sender
- **1 × PineCone BL602** as Gateway / Receiver
- **1 × PineCone BL602** as Sniffer
- **1 × Wi-Fi access point / router**
- USB serial connections for flashing and logs

### Gateway Extras
- **SSD1306 OLED display** (I2C)
- External LED + resistor (optional but recommended)

---

## Wiring

### SSD1306 OLED → PineCone (Gateway)

| OLED Pin | PineCone Pin |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | IO4 |
| SDA | IO3 |

### LED (Sender or Gateway)

| LED | PineCone |
|---|---|
| Anode (+) through resistor | GPIO5 |
| Cathode (–) | GND |



## Typical Runtime Flow

1. All boards boot and initialize FreeRTOS + lwIP.
2. All boards connect to the configured Wi-Fi network.
3. Receiver loads or generates its long-term ML-KEM key pair.
4. Sender broadcasts a CoAP request for `/pqkem-pk`.
5. Receiver responds with its public key.
6. Sender runs ML-KEM encapsulation and derives the AES key.
7. Sender encrypts the message and sends `/pqkem-data`.
8. Receiver decapsulates, derives the same AES key, authenticates, decrypts, and displays plaintext.
9. Sniffer captures the traffic for host-side observation.

## Future Work

The repository already demonstrates the core system, but the code and report also make clear what should come next:

- support **ML-KEM-768** and **ML-KEM-1024**
- move from the current compact custom payload format to **CBOR/COSE**
- align the system more closely with **OSCORE**
- add **key rotation**, **key revocation**, and **key recovery**
- study **multi-device** operation instead of only one Sender and one Gateway
- evaluate **congestion**, **packet loss**, and scaling behavior
- improve resistance to **DoS**, **packet flooding**, and **Wi-Fi jamming**
- add authenticated public-key distribution using **certificates**, **signatures**, or **key pinning**
- extend the sniffer tooling for anomaly detection and richer packet analysis

### Why It Matters Today

Organizations are already preparing for **Post-Quantum Cryptography (PQC)** migration. This project demonstrates that quantum-resistant communication can be implemented on low-power microcontrollers like **PineCone / BL602**, making it relevant for future long-lived IoT deployments where data captured today must remain secure tomorrow.
