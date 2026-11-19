#include "WioE5P2P.h"

WioE5P2P::WioE5P2P(Stream &serialPort) : _serial(serialPort) {
  _debug = nullptr;
  _bufIndex = 0;
  _isSleeping = false;
  _hasNewData = false;
  _lastId = 0;
  _lastType = TYPE_UNKNOWN;
  _lastPayloadLen = 0;
  _lastValue = 0;
  _lastFloat = 0.0f;
  _lastRssi = 0;
  _lastSnr = 0;

  for (uint8_t i = 0; i < WIO_MAX_TRACKED_SENSORS; i++) {
    _trackers[i].id = 0;
    _trackers[i].lastSeenMs = 0;
    _trackers[i].seenOnce = false;
  }
}

void WioE5P2P::begin(uint16_t freqMHz, WioSpreadingFactor sf, WioTxPower txPower, Stream *debugPort) {
  _debug = debugPort;
  wakeUp(); // Ensure module is awake in case the MCU was reset while Wio-E5 slept

  if (_debug) {
    _debug->println(F("=== Wio-E5 P2P Initialization ==="));
    _debug->print(F("Frequency: ")); _debug->print(freqMHz); _debug->println(F(" MHz"));
    _debug->print(F("Spreading Factor: SF")); _debug->println((uint8_t)sf);
    _debug->print(F("TX Power: ")); _debug->print((int8_t)txPower); _debug->println(F(" dBm"));
    _debug->println(F("Entering TEST mode..."));
  }

  _serial.println(F("AT+MODE=TEST"));
  logResponse(300);

  char cfgCmd[64];
  sprintf(cfgCmd, "AT+TEST=RFCFG,%u,SF%u,125,8,8,%d,ON,OFF,OFF", freqMHz, (uint8_t)sf, (int8_t)txPower);
  
  if (_debug) {
    _debug->print(F("Configuring RF: "));
    _debug->println(cfgCmd);
  }
  
  _serial.println(cfgCmd);
  logResponse(300);

  if (_debug) {
    _debug->println(F("=== Initialization Complete ==="));
  }
}

void WioE5P2P::sleep() {
  if (_debug) {
    _debug->println(F("Entering deep sleep (AT+LOWPOWER)..."));
  }
  _serial.println(F("AT+LOWPOWER"));
  logResponse(150);
  _isSleeping = true;
}

void WioE5P2P::wakeUp() {
  // Sending 0xFF bytes reliably wakes the STM32WLE5JC UART from deep sleep
  const uint8_t wakeBytes[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
  _serial.write(wakeBytes, 4);
  delay(10); // Wait 10 ms for UART peripheral to stabilize
  
  // Flush any "+LOWPOWER: WAKEUP" response from the serial buffer
  while (_serial.available()) {
    char c = _serial.read();
    if (_debug) _debug->write(c);
  }
  _isSleeping = false;
}

void WioE5P2P::setAutoLowPower(bool enable) {
  wakeUp();
  if (enable) {
    _serial.println(F("AT+LOWPOWER=AUTOON"));
  } else {
    _serial.println(F("AT+LOWPOWER=AUTOOFF"));
  }
  logResponse(200);
}

void WioE5P2P::prepareTx() {
  if (_isSleeping) {
    wakeUp();
  }
}

void WioE5P2P::logResponse(uint16_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (_serial.available()) {
      char c = _serial.read();
      if (_debug) {
        _debug->write(c);
      }
    }
  }
}

void WioE5P2P::send8(uint8_t sensorId, uint8_t value) {
  prepareTx();
  char txCmd[40];
  sprintf(txCmd, "AT+TEST=TXLRPKT,\"%02X%02X%02X\"", sensorId, TYPE_INT8, value);
  _serial.println(txCmd);
}

void WioE5P2P::send16(uint8_t sensorId, int16_t value) {
  prepareTx();
  uint8_t b1 = (value >> 8) & 0xFF;
  uint8_t b0 = value & 0xFF;

  char txCmd[40];
  sprintf(txCmd, "AT+TEST=TXLRPKT,\"%02X%02X%02X%02X\"", sensorId, TYPE_INT16, b1, b0);
  _serial.println(txCmd);
}

void WioE5P2P::send32(uint8_t sensorId, int32_t value) {
  prepareTx();
  uint8_t b3 = (value >> 24) & 0xFF;
  uint8_t b2 = (value >> 16) & 0xFF;
  uint8_t b1 = (value >> 8) & 0xFF;
  uint8_t b0 = value & 0xFF;

  char txCmd[40];
  sprintf(txCmd, "AT+TEST=TXLRPKT,\"%02X%02X%02X%02X%02X%02X\"", sensorId, TYPE_INT32, b3, b2, b1, b0);
  _serial.println(txCmd);
}

void WioE5P2P::sendFloat(uint8_t sensorId, float value) {
  prepareTx();
  uint8_t fBytes[4];
  memcpy(fBytes, &value, 4);

  char txCmd[40];
  sprintf(txCmd, "AT+TEST=TXLRPKT,\"%02X%02X%02X%02X%02X%02X\"", 
          sensorId, TYPE_FLOAT, fBytes[0], fBytes[1], fBytes[2], fBytes[3]);
  _serial.println(txCmd);
}

void WioE5P2P::startReceive() {
  wakeUp();
  if (_debug) {
    _debug->println(F("Starting continuous receive mode (RXLRPKT)..."));
  }
  _serial.println(F("AT+TEST=RXLRPKT"));
  logResponse(300);
}

bool WioE5P2P::available() {
  while (_serial.available()) {
    char c = _serial.read();

    if (c == '\n') {
      _rxBuffer[_bufIndex] = '\0';
      parseLine(_rxBuffer);
      _bufIndex = 0;
    } else if (c != '\r' && _bufIndex < sizeof(_rxBuffer) - 1) {
      _rxBuffer[_bufIndex++] = c;
    }
  }

  if (_hasNewData) {
    _hasNewData = false;
    return true;
  }
  return false;
}

void WioE5P2P::updateTracker(uint8_t sensorId) {
  // 1. Update timestamp if sensor ID is already tracked
  for (uint8_t i = 0; i < WIO_MAX_TRACKED_SENSORS; i++) {
    if (_trackers[i].seenOnce && _trackers[i].id == sensorId) {
      _trackers[i].lastSeenMs = millis();
      return;
    }
  }
  // 2. Otherwise allocate a free slot in the tracker array
  for (uint8_t i = 0; i < WIO_MAX_TRACKED_SENSORS; i++) {
    if (!_trackers[i].seenOnce) {
      _trackers[i].id = sensorId;
      _trackers[i].lastSeenMs = millis();
      _trackers[i].seenOnce = true;
      return;
    }
  }
}

bool WioE5P2P::isAlive(uint8_t sensorId, uint32_t timeoutMs) {
  for (uint8_t i = 0; i < WIO_MAX_TRACKED_SENSORS; i++) {
    if (_trackers[i].seenOnce && _trackers[i].id == sensorId) {
      return (millis() - _trackers[i].lastSeenMs) <= timeoutMs;
    }
  }
  // Sensor has never been seen since startup
  return false;
}

uint32_t WioE5P2P::getTimeSinceLastSeen(uint8_t sensorId) {
  for (uint8_t i = 0; i < WIO_MAX_TRACKED_SENSORS; i++) {
    if (_trackers[i].seenOnce && _trackers[i].id == sensorId) {
      return millis() - _trackers[i].lastSeenMs;
    }
  }
  return 0xFFFFFFFF; // Maximum value indicates "never seen"
}

void WioE5P2P::resetSensorTracker(uint8_t sensorId) {
  for (uint8_t i = 0; i < WIO_MAX_TRACKED_SENSORS; i++) {
    if (_trackers[i].seenOnce && _trackers[i].id == sensorId) {
      _trackers[i].seenOnce = false;
      _trackers[i].lastSeenMs = 0;
      return;
    }
  }
}

void WioE5P2P::parseLine(const char* line) {
  // 1. Extract RSSI and SNR from "+TEST: LEN:6, RSSI:-45, SNR:11"
  if (strncmp(line, "+TEST: LEN:", 11) == 0) {
    const char* rssiPtr = strstr(line, "RSSI:");
    if (rssiPtr != nullptr) {
      _lastRssi = (int16_t)atoi(rssiPtr + 5);
    }
    const char* snrPtr = strstr(line, "SNR:");
    if (snrPtr != nullptr) {
      _lastSnr = (int8_t)atoi(snrPtr + 4);
    }
    return;
  }

  // 2. Extract hex payload from "+TEST: RX \"...\""
  const char* prefix = "+TEST: RX \"";
  if (strncmp(line, prefix, 11) == 0) {
    const char* hexStr = line + 11;
    uint8_t data[16];
    uint8_t byteCount = 0;

    for (uint8_t i = 0; hexStr[i] != '\0' && hexStr[i] != '"' && hexStr[i + 1] != '\0' && byteCount < 16; i += 2) {
      char hexByte[3] = { hexStr[i], hexStr[i + 1], '\0' };
      data[byteCount++] = (uint8_t)strtol(hexByte, NULL, 16);
    }

    // Minimum 3 bytes required (1 byte ID + 1 byte type + at least 1 byte payload)
    if (byteCount >= 3) {
      _lastId = data[0];
      _lastType = (WioDataType)data[1];

      if (_lastType == TYPE_INT8 && byteCount == 3) {
        _lastPayloadLen = 1;
        _lastValue = (uint8_t)data[2];
        _lastFloat = (float)_lastValue;
        _hasNewData = true;
        updateTracker(_lastId);
      } 
      else if (_lastType == TYPE_INT16 && byteCount == 4) {
        _lastPayloadLen = 2;
        _lastValue = (int16_t)(((uint16_t)data[2] << 8) | data[3]);
        _lastFloat = (float)_lastValue;
        _hasNewData = true;
        updateTracker(_lastId);
      } 
      else if (_lastType == TYPE_INT32 && byteCount == 6) {
        _lastPayloadLen = 4;
        _lastValue = ((int32_t)data[2] << 24) | 
                     ((int32_t)data[3] << 16) | 
                     ((int32_t)data[4] << 8)  | 
                     (int32_t)data[5];
        _lastFloat = (float)_lastValue;
        _hasNewData = true;
        updateTracker(_lastId);
      }
      else if (_lastType == TYPE_FLOAT && byteCount == 6) {
        _lastPayloadLen = 4;
        uint8_t fBytes[4] = { data[2], data[3], data[4], data[5] };
        memcpy(&_lastFloat, fBytes, 4);
        _lastValue = (int32_t)_lastFloat;
        _hasNewData = true;
        updateTracker(_lastId);
      }
    }
  }
}

uint8_t WioE5P2P::getLastSensorId() { return _lastId; }
WioDataType WioE5P2P::getLastDataType() { return _lastType; }
uint8_t WioE5P2P::getLastPayloadLength() { return _lastPayloadLen; }
int32_t WioE5P2P::getLastValue() { return _lastValue; }
float WioE5P2P::getLastFloat() { return _lastFloat; }
int16_t WioE5P2P::getLastRssi() { return _lastRssi; }
int8_t WioE5P2P::getLastSnr() { return _lastSnr; }

