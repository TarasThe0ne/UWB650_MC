
# UWB650 Reference Documentation (LLM Friendly)

Source documents:
- UWB650 Serial Ranging Communication and Positioning Module Rev 2.2
- UWB650 Serial Ranging Communication and Positioning Module Rev 1.1

Chipset: Qorvo DW3000
Standard: IEEE 802.15.4-2020 UWB
Channel: 5 (6489.6 MHz)

---

# 1. Overview

UWB650 is a UWB wireless communication module integrating:

- DW3000 UWB chip
- RF power amplifier (up to 0.5 W)
- MCU controller
- UART interface
- ranging and positioning firmware

Functions:

- UWB wireless data transmission
- distance measurement (Two-Way Ranging)
- indoor positioning
- mesh networking
- AES-128 encryption

Maximum open field communication distance: up to ~1 km.

---

# 2. Hardware Parameters

## Electrical

| Parameter | Value |
|-----------|------|
| Supply voltage | 3.0 – 5.5 V |
| Operating temperature | -20°C – 60°C |
| Sleep current | <2.3 mA |
| RX current | ~100 mA |
| TX current | ~300 mA (continuous frame mode) |

## RF

| Parameter | Value |
|-----------|------|
| Frequency | 6489.6 MHz |
| RF data rate | 850 Kbps / 6.8 Mbps |
| Bandwidth | 499.2 MHz |
| RX sensitivity | -100 dBm (850K) / -94 dBm (6.8M) |
| Max TX power | ~27 dBm |

---

# 3. UART Interface

Default serial configuration:

| Parameter | Value |
|-----------|------|
| Baud rate | 115200 |
| Data bits | 8 |
| Stop bits | 1 |
| Parity | None |

Commands must:

- start with `UWBRFAT`
- end with `CR LF`

Example:

UWBRFAT\r\n

Response:

OK

---

# 4. Important Pins

| Pin | Name | Description |
|----|------|-------------|
| 3 | RESET | Reset module |
| 13 | CS | Sleep control |
| 15 | RXD | UART RX |
| 16 | TXD | UART TX |
| 21 | TXOK | TX status |
| 22 | RXOK | RX status |
| 23 | P011 | Ranging status |
| 35 | UPGRADE | Firmware update mode |

---

# 5. Core Commands

## Check module

UWBRFAT

Response

OK

---

## Firmware version

UWBRFAT+VERSION?

Example response

V1.2

---

## Reset module

UWBRFAT+RESET

---

## Restore factory settings

UWBRFAT+DEFAULT

---

## Save parameters to flash

UWBRFAT+FLASH

---

# 6. Serial Configuration

Query baud rate

UWBRFAT+BAUDRATE?

Set baud rate

UWBRFAT+BAUDRATE=<rate>

Rates:

| Value | Baud |
|------|------|
|0|230400|
|1|115200 (default)|
|2|57600|
|3|38400|
|4|19200|
|5|9600|

---

# 7. Device ID Configuration

Each module belongs to a PAN network.

Command:

UWBRFAT+DEVICEID=<PANID>,<ADDR>

Example:

UWBRFAT+DEVICEID=ABCD,1234

Rules:

- PAN ID must match for communication
- Address must be unique

Range:

0000 – FFFE

---

# 8. Transmit Power

Command:

UWBRFAT+POWER=<gear>

| Gear | Power |
|------|------|
|0|-5 dBm|
|5|11 dBm|
|10|~27 dBm (default)|

---

# 9. Data Transmission

Set target address:

UWBRFAT+TXTARGET=<address>

Broadcast address:

FFFF

Enable ACK:

UWBRFAT+ACKENABLE=1

Enable channel assessment:

UWBRFAT+CCAENABLE=1

---

# 10. Ranging (Distance Measurement)

Command:

UWBRFAT+RANGING=<count>,<addr>

Example:

UWBRFAT+RANGING=1,0001

Response:

+RANGING=(12.34),(-56.78)

Where:

distance = meters  
rssi = signal strength (dBm)

If ranging fails:

+RANGING=(-1),(0.00)

---

# 11. Positioning

Command:

UWBRFAT+LOCATION=<addr1>,<addr2>,<addr3>

Example:

UWBRFAT+LOCATION=0001,0002,0003

Response:

+LOCATION=(x,y,z),(distance),(rssi)

Units:

meters

Minimum anchors:

- 3 anchors → 2D positioning
- 4 anchors → 3D positioning

---

# 12. Antenna Delay Calibration

Command:

UWBRFAT+ANTDELAY=<delay>

Default:

16400

Recommended values:

| Antenna | Delay |
|--------|------|
|UWB-PCB-X|16433|
|UWB-PCB-D|16476|
|UWB-ZT50|16408|

Calibration improves ranging accuracy.

---

# 13. Encryption

Enable AES:

UWBRFAT+SECURITY=1,<key>

Key:

32 hex characters (128-bit).

---

# 14. Quick Start (2 modules)

Module A:

UWBRFAT+DEVICEID=0001,0001

Module B:

UWBRFAT+DEVICEID=0001,0002

Start ranging:

UWBRFAT+RANGING=1,0002

Example response:

+RANGING=(5.32),(-48.11)

---

# 15. Known Limitations

Metal objects strongly absorb UWB signals.

Glass degrades ranging accuracy.

Walls cannot be reliably penetrated.

Anchor placement recommended >2 m height.

---

# 16. Power Saving

Sleep:
pull CS pin LOW

Wake:
set CS pin HIGH

---

End of document
