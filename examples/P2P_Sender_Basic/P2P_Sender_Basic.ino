#include <SoftwareSerial.h>
#include <WioE5P2P.h>

const uint8_t WIO_RX_PIN = 4; // Connected to Wio-E5 TX
const uint8_t WIO_TX_PIN = 5; // Connected to Wio-E5 RX (via 3.3V level shifter)

SoftwareSerial wioSerial(WIO_RX_PIN, WIO_TX_PIN);
WioE5P2P lora(wioSerial);

// Unique ID for this sensor node (1 to 255)
const uint8_t MY_SENSOR_ID = 1;

void setup() {
  Serial.begin(9600);
  wioSerial.begin(9600);

  // Initialize with 868 MHz, SF7, 14 dBm, and debug output on Serial
  lora.begin(868, WIO_SF7, WIO_PWR_14DBM, &Serial);
}

void loop() {
  // Example 1: Send 8-bit unsigned integer (0..255, e.g., humidity in %)
  uint8_t humidity = 62;
  Serial.println(F("Sending 8-bit value..."));
  lora.send8(MY_SENSOR_ID, humidity);
  delay(5000);

  // Example 2: Send 16-bit signed integer (-32768..32767, e.g., battery voltage in mV)
  int16_t voltage_mV = 3280;
  Serial.println(F("Sending 16-bit value..."));
  lora.send16(MY_SENSOR_ID, voltage_mV);
  delay(5000);

  // Example 3: Send 32-bit signed integer (e.g., barometric pressure in Pa)
  int32_t pressure_Pa = 101325;
  Serial.println(F("Sending 32-bit value..."));
  lora.send32(MY_SENSOR_ID, pressure_Pa);
  delay(5000);

  // Example 4: Send 32-bit floating-point value (e.g., temperature in °C)
  float temperature = 23.45;
  Serial.println(F("Sending float value..."));
  lora.sendFloat(MY_SENSOR_ID, temperature);
  delay(5000);
}

