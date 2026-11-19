#ifndef WIO_E5_P2P_H
#define WIO_E5_P2P_H

#include <Arduino.h>
#include <Stream.h>

// Maximum number of tracked sensor IDs for timeout monitoring (saves RAM on AVR)
#define WIO_MAX_TRACKED_SENSORS 16

enum WioDataType : uint8_t {
  TYPE_UNKNOWN = 0,
  TYPE_INT8    = 1,
  TYPE_INT16   = 2,
  TYPE_INT32   = 3,
  TYPE_FLOAT   = 4
};

// Allowed LoRa spreading factors (SF7 = fast/short range, SF12 = slow/maximum range)
enum WioSpreadingFactor : uint8_t {
  WIO_SF7  = 7,
  WIO_SF8  = 8,
  WIO_SF9  = 9,
  WIO_SF10 = 10,
  WIO_SF11 = 11,
  WIO_SF12 = 12
};

// Common TX power levels in dBm (14 dBm is the standard EU limit at 868 MHz)
enum WioTxPower : int8_t {
  WIO_PWR_MIN    = -1,
  WIO_PWR_2DBM   = 2,
  WIO_PWR_5DBM   = 5,
  WIO_PWR_8DBM   = 8,
  WIO_PWR_10DBM  = 10,
  WIO_PWR_14DBM  = 14,
  WIO_PWR_17DBM  = 17,
  WIO_PWR_20DBM  = 20,
  WIO_PWR_22DBM  = 22
};

struct SensorTracker {
  uint8_t id;
  uint32_t lastSeenMs;
  bool seenOnce;
};

class WioE5P2P {
  public:
    WioE5P2P(Stream &serialPort);

    // Initializes P2P test mode with frequency (MHz), spreading factor, and TX power
    void begin(uint16_t freqMHz = 868, 
               WioSpreadingFactor sf = WIO_SF7, 
               WioTxPower txPower = WIO_PWR_14DBM, 
               Stream *debugPort = nullptr);

    // Transmit functions for 1-, 2-, and 4-byte integers as well as 4-byte floats
    void send8(uint8_t sensorId, uint8_t value);
    void send16(uint8_t sensorId, int16_t value);
    void send32(uint8_t sensorId, int32_t value);
    void sendFloat(uint8_t sensorId, float value);

    // Power-saving functions (for transmitter nodes only)
    void sleep();
    void wakeUp();
    void setAutoLowPower(bool enable);

    // Receiver control
    void startReceive();
    bool available();

    // Retrieve decoded payload values
    uint8_t getLastSensorId();
    WioDataType getLastDataType();
    uint8_t getLastPayloadLength();
    int32_t getLastValue();
    float getLastFloat();

    // Retrieve signal quality metrics
    int16_t getLastRssi();
    int8_t getLastSnr();

    // Connection timeout monitoring
    bool isAlive(uint8_t sensorId, uint32_t timeoutMs);
    uint32_t getTimeSinceLastSeen(uint8_t sensorId);
    void resetSensorTracker(uint8_t sensorId);

  private:
    Stream &_serial;
    Stream *_debug;
    char _rxBuffer[80];
    uint8_t _bufIndex;
    bool _isSleeping;

    bool _hasNewData;
    uint8_t _lastId;
    WioDataType _lastType;
    uint8_t _lastPayloadLen;
    int32_t _lastValue;
    float _lastFloat;
    int16_t _lastRssi;
    int8_t _lastSnr;

    SensorTracker _trackers[WIO_MAX_TRACKED_SENSORS];

    void parseLine(const char* line);
    void logResponse(uint16_t timeoutMs);
    void prepareTx();
    void updateTracker(uint8_t sensorId);
};

#endif

