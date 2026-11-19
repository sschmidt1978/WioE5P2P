# WioE5P2P – Arduino Library for Seeed Wio-E5 (STM32WLE5JC)

A lightweight Arduino library for direct **point-to-point (P2P) LoRa communication** using the **Seeed Wio-E5 / LoRa-E5** via AT commands — completely independent of any LoRaWAN gateway, network server, or OTAA/ABP activation.

## Features

* **Flexible Data Types:** Automatically transmits and decodes `uint8_t` (1 byte), `int16_t` (2 bytes), `int32_t` (4 bytes), and `float` (4 bytes).
* **Sensor ID Management:** Assign a unique ID (`0`–`255`) to each transmitter node to distinguish multiple sensors on a single receiver.
* **Signal Quality Metrics:** Extracts **RSSI** (in dBm) and **SNR** (in dB) for every received packet.
* **Type-Safe Configuration:** Enums for Spreading Factor (`WIO_SF7` to `WIO_SF12`) and TX Power (`WIO_PWR_MIN` to `WIO_PWR_22DBM`).
* **Low-Power Sleep Mode:** Supports `AT+LOWPOWER` (~2.1 µA) with automatic wake-up before transmission.
* **Connection Monitoring:** Built-in timeout tracking (`isAlive()`) for up to 16 sensor IDs simultaneously.
* **Debug Logging:** Optional debug stream (`&Serial`) prints initialization steps and module responses.

---

## Hardware Wiring (Arduino Nano & Wio-E5)

> **Important Note:** The Seeed Wio-E5 operates at **3.3 V** logic and power levels, while the Arduino Nano uses **5 V**. Never connect the Wio-E5 directly to 5 V, and always use a logic level shifter or voltage divider on the Arduino TX line.

| Arduino Nano | Level Shifting / Note | Seeed Wio-E5 |
| :--- | :--- | :--- |
| **3.3V** (or external 3.3V regulator) | Direct (max. 3.6 V) | **VCC / 3V3** |
| **GND** | Direct | **GND** |
| **Pin 4** (SoftwareSerial RX) | Direct (3.3 V is sufficient for Arduino HIGH) | **TX** |
| **Pin 5** (SoftwareSerial TX) | **Voltage divider (e.g., 1 kΩ / 2 kΩ)** to 3.3 V | **RX** |

*(Note: Always ensure an 868 MHz antenna is connected to the Wio-E5 before transmitting.)*

---

## Installation

1. Copy the entire `WioE5P2P` folder into your Arduino libraries directory:
   * **Windows:** `Documents\Arduino\libraries\WioE5P2P\`
   * **macOS / Linux:** `~/Documents/Arduino/libraries/WioE5P2P/`
2. Restart the Arduino IDE.
3. Open the included examples via **File → Examples → WioE5P2P**.
   *(Note: The `P2P_Sender_LowPower` example requires the **Low-Power** library from the Arduino Library Manager.)*

---

## API Overview

### Initialization
```cpp
WioE5P2P lora(wioSerial);
lora.begin(868, WIO_SF7, WIO_PWR_14DBM, &Serial);
```
* `freqMHz`: Carrier frequency in MHz (e.g., `868` for Europe).
* `sf`: Spreading factor (`WIO_SF7`, `WIO_SF8`, `WIO_SF9`, `WIO_SF10`, `WIO_SF11`, `WIO_SF12`).
* `txPower`: Transmit power (`WIO_PWR_MIN`, `WIO_PWR_2DBM`, `WIO_PWR_5DBM`, `WIO_PWR_8DBM`, `WIO_PWR_10DBM`, `WIO_PWR_14DBM`, `WIO_PWR_17DBM`, `WIO_PWR_20DBM`, `WIO_PWR_22DBM`). *(In the EU, `14 dBm` is the legal limit on 868 MHz.)*
* `debugPort`: Optional pointer to a `Stream` (e.g., `&Serial`) for status logs.

### Transmitting Data (Sensor Node)
```cpp
lora.send8(sensorId, 65);         // 1 byte (uint8_t)
lora.send16(sensorId, -450);      // 2 bytes (int16_t)
lora.send32(sensorId, 101325);    // 4 bytes (int32_t)
lora.sendFloat(sensorId, 23.75);  // 4 bytes (float)
```

### Receiving Data (Base Station)
```cpp
lora.startReceive(); // Call once in setup()

if (lora.available()) {
  uint8_t id      = lora.getLastSensorId();
  WioDataType typ = lora.getLastDataType(); // TYPE_INT8, TYPE_INT16, TYPE_INT32, TYPE_FLOAT
  int32_t val     = lora.getLastValue();
  float fVal      = lora.getLastFloat();
  int16_t rssi    = lora.getLastRssi();
  int8_t snr      = lora.getLastSnr();
}
```

### Connection Timeout Monitoring
```cpp
bool online = lora.isAlive(sensorId, 12000);         // Returns true if seen within 12 seconds
uint32_t ageMs = lora.getTimeSinceLastSeen(sensorId); // Milliseconds since last packet
lora.resetSensorTracker(sensorId);                   // Clears tracking state for a sensor ID
```

### Power Saving (Transmitter Only)
```cpp
lora.sleep();  // Puts the Wio-E5 into deep sleep (~2.1 µA)
lora.wakeUp(); // Manually wakes the module (also triggered automatically by send...())
```

---

## Packet Frame Format

Every radio packet is transmitted internally as a hex string via `AT+TEST=TXLRPKT` using the following layout:

| Byte 0 | Byte 1 | Byte 2 .. 5 |
| :--- | :--- | :--- |
| **Sensor ID** (`0x00`–`0xFF`) | **Data Type** (`1`=INT8, `2`=INT16, `3`=INT32, `4`=FLOAT) | **Payload** (1, 2, or 4 bytes) |

