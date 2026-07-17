#include <Arduino.h>

#define PIN_CLK D2
#define PIN_DAT D1

#define MESSAGE_WIDTH 80

uint32_t dataDuringClockChange[84];
uint32_t dataDuringClockChangePointer = 0;
uint32_t clockLevel;
uint32_t lstClockChange;
uint32_t clockChanges = 0;

// A request/response cycle is two consecutive messages on the bus: the tester
// sends the request, then the ODU clocks back its response. We buffer the
// request until the response arrives, then print them together.
uint32_t request[80];

void setup() {
  HWCDCSerial.begin();
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DAT, INPUT_PULLUP);
  delay(2000);
  clockLevel = digitalRead(PIN_CLK);
  lstClockChange = millis();
}

bool allHigh(uint32_t *bits) {
  for (uint32_t i = 0; i < MESSAGE_WIDTH; i++) {
    if (bits[i] == 0) {
      return false;
    }
  }
  return true;
}

// Assembles a byte from the wire bits LSB-first, matching how the ODU
// interprets a frame (the first bit clocked is bit 0).
static uint8_t byteFromBits(const uint32_t *bits, uint32_t byteIndex) {
  uint8_t b = 0;
  for (uint32_t bitIndex = 0; bitIndex < 8; bitIndex++) {
    if (bits[byteIndex * 8 + bitIndex]) b |= 1 << bitIndex;
  }
  return b;
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

void printHex(uint32_t *message) {
  HWCDCSerial.print("0x");
  for (uint32_t byteIndex = 0; byteIndex < 80 / 8; byteIndex++) {
    uint8_t b = byteFromBits(message, byteIndex);
    if (b < 0x10) HWCDCSerial.print('0');  // keep the leading zero
    HWCDCSerial.print(b, HEX);
  }
}

// Read LSB-first, a request opens with the header byte 0xAA, a response with
// 0x55. Rebuild the first byte from its bits and check for the request header.
boolean isRequest(uint32_t *message) {
  return byteFromBits(message, 0) == 0xAA;
}

void handleMessage(uint32_t *message, uint32_t count) {
  
  if (isRequest(message)) {
    for (uint32_t i = 0; i < count; i++) {
      request[i] = message[count == 84 ? 4 : 0 + i];
    }
    return;
  }
  // second message of a cycle -> the ODU's response; emit the pair
  HWCDCSerial.print("req=");
  printHex(request);
  HWCDCSerial.print(", res=");
  printHex(message);
  if (allHigh(message)) {
      HWCDCSerial.println(", status=NO_RESPONSE_FROM_ODU");
    } else if ( ! checksumValid(request) || ! checksumValid(message)) {
      HWCDCSerial.println(", status=CHECKSUM_ERROR");
    } else {
      HWCDCSerial.println(", status=OK");
    }
}

void loop() {
  
  uint32_t newClockLevel = digitalRead(PIN_CLK);
  
  if (clockLevel != newClockLevel) {
  
    lstClockChange = millis();
    clockLevel = newClockLevel;
    
    if (clockChanges % 2 == 1) {

      // Capture the level of the data line in the middle of a clock cycle
      // (e.g., every other time the level changes from HIGH to LOW or vice versa):
      //
      //                 ↓         ↓
      //  ----------|    |----|    |----|   
      //            |    |    |    |    |    
      //            |----|    |----|    |----
      //      
      dataDuringClockChange[dataDuringClockChangePointer] = digitalRead(PIN_DAT);
      dataDuringClockChangePointer++;

    }
    
    clockChanges++;

    // A clock burst changes the level of the clock pin every ms. If there is no change since 10ms, 
    // we've either seen a completed message or an irrelevant change of the clock signal
  
  } else if ((millis() - lstClockChange > 10) && clockChanges > 0) {

    if (clockChanges == 1) {

      // noise -> discard

    } else if (clockChanges == 169) {

      // 4 zero bits from the handshake + 80-bit wide message
      handleMessage(dataDuringClockChange, 84);

    } else if (clockChanges == 161) {

      // regular 80-bit message
      handleMessage(dataDuringClockChange, 80);

    } else {
      
      // incomplete message
      HWCDCSerial.println("req=                      , res=                      , status=INCOMPLETE");

    }

    clockChanges = 0;
    dataDuringClockChangePointer = 0;

  }

}
