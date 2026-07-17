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

// Assembles a byte from the wire bits LSB-first, matching how the ODU
// interprets a frame (the first bit clocked is bit 0).
static uint8_t byteFromBits(const uint32_t *bits, size_t byteIndex) {
  uint8_t b = 0;
  for (int bitIndex = 0; bitIndex < 8; bitIndex++) {
    if (bits[byteIndex * 8 + bitIndex]) b |= 1 << bitIndex;
  }
  return b;
}

// Prints the message as hex without a trailing newline; the caller ends the line.
void printBitsAsHex(uint32_t *bits) {
  for (size_t byteIndex = 0; byteIndex < MESSAGE_WIDTH / 8; byteIndex++) {
    uint8_t b = byteFromBits(bits, byteIndex);
    if (b < 0x10) HWCDCSerial.print('0');  // keep the leading zero
    HWCDCSerial.print(b, HEX);
  }
}

// The last byte is a checksum: with bytes read LSB-first the way the ODU does,
// a frame is valid exactly when payload and checksum sum to zero modulo 256.
bool checksumValid(uint32_t *bits) {
  uint8_t sum = 0;
  for (size_t byteIndex = 0; byteIndex < MESSAGE_WIDTH / 8; byteIndex++) {
    sum += byteFromBits(bits, byteIndex);
  }
  return sum == 0;
}

void printBytesAsHex(const uint8_t *bytes) {
  for (size_t i = 0; i < MESSAGE_WIDTH / 8; i++) {
    if (bytes[i] < 0x10) HWCDCSerial.print('0');  // keep the leading zero
    HWCDCSerial.print(bytes[i], HEX);
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
    
    // send request, LSB-first
    for (size_t byteIndex = 0; byteIndex < MESSAGE_WIDTH / 8; byteIndex++) {
      for (int bitIndex = 0; bitIndex < 8; bitIndex++) {
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
    HWCDCSerial.print("req=0x");
    printBytesAsHex(request);
    HWCDCSerial.print(", res=0x");
    printBitsAsHex(responseBits);
    
    if (allHigh(responseBits)) {
      HWCDCSerial.println(", status=NO_RESPONSE_FROM_ODU");
    } else if ( ! checksumValid(responseBits)) {
      HWCDCSerial.println(", status=CHECKSUM_ERROR");
    } else {
      HWCDCSerial.println(", status=OK");
    }    

    if ( ! allHigh(responseBits)) {
      delay(56);
      break;
    }
    delay(500);
  } while (true);
}

void setup() {
  HWCDCSerial.begin();
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
      {0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56},
      {0xAA, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55},
      {0xAA, 0x02, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x55},
      {0xAA, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53},
    };
    for (auto &request : requests) {
      requestResponseCycle(request);
    }
}
