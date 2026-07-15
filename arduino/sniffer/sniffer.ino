#include <Arduino.h>

#define PIN_CLK D2
#define PIN_DAT D1

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
  Serial.begin(115200);
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DAT, INPUT_PULLUP);
  delay(2000);
  clockLevel = digitalRead(PIN_CLK);
  lstClockChange = millis();
}

void printHex(uint32_t *message) {
  Serial.print("0x");
  uint8_t nibble = 0;
  for (uint32_t i = 0; i < 80; i++) {
    nibble = (nibble << 1) | (message[i] ? 1 : 0);
    if (i % 4 == 3) {
      Serial.print(nibble, HEX);
      nibble = 0;
    }
  }
}

// A request opens with the header byte 0x55, a response with 0xAA. Rebuild the
// first byte from its bits and check for the request header.
boolean isRequest(uint32_t *message) {
  uint8_t header = 0;
  for (uint32_t i = 0; i < 8; i++) {
    header = (header << 1) | (message[i] ? 1 : 0);
  }
  return header == 0x55;
}

void handleMessage(uint32_t *message, uint32_t count) {
  
  if (isRequest(message)) {
    for (uint32_t i = 0; i < count; i++) {
      request[i] = message[count == 84 ? 4 : 0 + i];
    }
    return;
  }
  // second message of a cycle -> the ODU's response; emit the pair
  Serial.print("req=");
  printHex(request);
  Serial.print(", res=");
  printHex(message);
  Serial.println();
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
      Serial.println(String("req=                      , res=                       (discarded partial message with ") + ((clockChanges - 1) / 2) +  String(" bits)"));


    }

    clockChanges = 0;
    dataDuringClockChangePointer = 0;

  }

}
