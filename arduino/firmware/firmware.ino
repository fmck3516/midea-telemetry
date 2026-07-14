#include <Arduino.h>

#define PIN_CLK D2
#define PIN_DAT D1

#define HALF_PERIOD_US 1000u
#define MESSAGE_WIDTH 80

uint32_t responseBits[MESSAGE_WIDTH];
bool firstIteration = true;

static inline void setLine(uint8_t pin, uint8_t level) {
  if (level)
    pinMode(pin, INPUT);  // release - bus pull-up brings it high
  else {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
}

void sendBit(uint8_t bit) {
  setLine(PIN_DAT, bit);
  setLine(PIN_CLK, 0);
  delayMicroseconds(HALF_PERIOD_US);
  setLine(PIN_CLK, 1);
  delayMicroseconds(HALF_PERIOD_US);
}

uint8_t readBit() {
  pinMode(PIN_DAT, INPUT);  // let the ODU drive it instead of us
  setLine(PIN_CLK, 1);
  delayMicroseconds(HALF_PERIOD_US);
  uint8_t bit = digitalRead(PIN_DAT);  // sample edge - flip if this reads wrong
  setLine(PIN_CLK, 0);
  delayMicroseconds(HALF_PERIOD_US);
  return bit;
}

void printBitsAsHex(uint32_t *bits) {
  uint8_t nibble = 0;
  for (uint32_t i = 0; i < MESSAGE_WIDTH; i++) {
    nibble = (nibble << 1) | (bits[i] ? 1 : 0);
    if (i % 4 == 3) {
      Serial.print(nibble, HEX);
      nibble = 0;
    }
  }
  Serial.println();
}

void printBytesAsHex(const uint8_t *bytes) {
  for (size_t i = 0; i < MESSAGE_WIDTH / 8; i++) {
    if (bytes[i] < 0x10) Serial.print('0');  // keep the leading zero
    Serial.print(bytes[i], HEX);
  }
}

bool allHigh(uint32_t *bits) {
  for (uint32_t i = 0; i < MESSAGE_WIDTH; i++) {
    if (bits[i] == 0) {
      return false;
    }
  }
  return true;
}

void requestResponseCycle(const uint8_t *request) {
  do {
    
    // send request
    for (size_t byteIndex = 0; byteIndex < MESSAGE_WIDTH / 8; byteIndex++) {
      for (int bitIndex = 7; bitIndex >= 0; bitIndex--) {
        sendBit((request[byteIndex] >> bitIndex) & 1);
      }
    }
    setLine(PIN_CLK, 0);
    setLine(PIN_DAT, 1);
    delay(60);
    
    // read response
    for (int i = 0; i < MESSAGE_WIDTH; i++) {
      responseBits[i] = readBit();
    }
    setLine(PIN_CLK, 1);
    
    // print request/response pair to serial
    Serial.print("req=0x");
    printBytesAsHex(request);
    Serial.print(", res=0x");
    printBitsAsHex(responseBits);

    if ( ! allHigh(responseBits)) {
      delay(56);
      break;
    }
    delay(500);
  } while (true);
}

void setup() {
  Serial.begin(115200);
  delay(5000);
}

void loop() {
    if (firstIteration) {
      setLine(PIN_CLK, 0);
      setLine(PIN_DAT, 0);
      delay(1000);
      setLine(PIN_CLK, 1);
      setLine(PIN_DAT, 1);
      delay(43);
      sendBit(0);
      sendBit(0);
      sendBit(0);
      sendBit(0);
      firstIteration = false;
    }
    static const uint8_t requests[][MESSAGE_WIDTH / 8] = {
      {0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6A},
      {0x55, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA},
      {0x55, 0x40, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xAA},
      {0x55, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCA},
    };
    for (auto &request : requests) {
      requestResponseCycle(request);
    }
}
