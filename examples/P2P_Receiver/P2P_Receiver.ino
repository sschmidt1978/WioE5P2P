#include <SoftwareSerial.h>
#include <WioE5P2P.h>

const uint8_t LED_PIN    = 2; // Status LED on D2
const uint8_t WIO_RX_PIN = 4; // Connected to Wio-E5 TX
const uint8_t WIO_TX_PIN = 5; // Connected to Wio-E5 RX (via 3.3V level shifter)

SoftwareSerial wioSerial(WIO_RX_PIN, WIO_TX_PIN);
WioE5P2P lora(wioSerial);

const uint8_t MONITORED_SENSOR_ID = 1;
const uint32_t TIMEOUT_MS = 12000; // 12 seconds timeout

uint8_t lastSensorState = 0;
uint32_t lastBlinkMs = 0;
bool blinkState = false;

void setup() {
  Serial.begin(9600);
  wioSerial.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  lora.begin(868, WIO_SF7, WIO_PWR_14DBM, &Serial);
  lora.startReceive();
}

void loop() {
  // 1. Check for incoming packets
  if (lora.available()) {
    uint8_t id = lora.getLastSensorId();

    Serial.print(F("Sensor ID: "));
    Serial.print(id);
    Serial.print(F(" | Value: "));

    if (lora.getLastDataType() == TYPE_FLOAT) {
      Serial.print(lora.getLastFloat(), 2);
    } else {
      Serial.print(lora.getLastValue());
    }

    Serial.print(F(" | RSSI: "));
    Serial.print(lora.getLastRssi());
    Serial.print(F(" dBm | SNR: "));
    Serial.print(lora.getLastSnr());
    Serial.println(F(" dB"));

    if (id == MONITORED_SENSOR_ID) {
      lastSensorState = (lora.getLastValue() == 1) ? 1 : 0;
    }
  }

  // 2. Check connection timeout for monitored sensor
  if (!lora.isAlive(MONITORED_SENSOR_ID, TIMEOUT_MS)) {
    // Error state: blink LED every 250 ms
    if (millis() - lastBlinkMs >= 250) {
      lastBlinkMs = millis();
      blinkState = !blinkState;
      digitalWrite(LED_PIN, blinkState ? HIGH : LOW);
    }
  } else {
    // Normal operation: LED reflects sensor state
    digitalWrite(LED_PIN, (lastSensorState == 1) ? HIGH : LOW);
  }
}

