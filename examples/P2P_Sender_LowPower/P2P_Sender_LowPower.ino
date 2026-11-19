#include <SoftwareSerial.h>
#include <LowPower.h> // Install "Low-Power" via Arduino Library Manager
#include <WioE5P2P.h>

const uint8_t WIO_RX_PIN = 4; // Connected to Wio-E5 TX
const uint8_t WIO_TX_PIN = 5; // Connected to Wio-E5 RX (via 3.3V level shifter)

SoftwareSerial wioSerial(WIO_RX_PIN, WIO_TX_PIN);
WioE5P2P lora(wioSerial);

const uint8_t MY_SENSOR_ID = 1;
const uint8_t SLEEP_CYCLES = 7; // 7 * 8s = 56 seconds sleep interval

void setup() {
  Serial.begin(9600);
  wioSerial.begin(9600);
  
  lora.begin(868, WIO_SF7, WIO_PWR_14DBM, &Serial);
}

void loop() {
  float temperature = 21.85;

  // Automatically wakes Wio-E5 and transmits the payload
  lora.sendFloat(MY_SENSOR_ID, temperature);
  delay(200); // Wait for transmission to complete (increase for higher SF)

  // Put Wio-E5 into deep sleep (~2.1 uA)
  lora.sleep();
  delay(50);

  // Keep UART TX line HIGH so Wio-E5 stays asleep
  digitalWrite(WIO_TX_PIN, HIGH);
  Serial.flush();

  // Put Arduino Nano into power-down mode
  for (uint8_t i = 0; i < SLEEP_CYCLES; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
}

