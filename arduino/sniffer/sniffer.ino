#include <Arduino.h>

#define PIN_CLK D2
#define PIN_DAT D1

uint32_t dataDuringClockChange[84];
uint32_t dataDuringClockChangePointer = 0;
uint32_t clockLevel;
uint32_t lstClockChange;
uint32_t clockChanges = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DAT, INPUT_PULLUP);
  delay(2000);
  clockLevel = digitalRead(PIN_CLK);
  lstClockChange = millis();
}

void printBitsAsHex(uint32_t *bits, uint32_t count) {
  uint8_t nibble = 0;
  for (uint32_t i = 0; i < count; i++) {
    nibble = (nibble << 1) | (bits[i] ? 1 : 0);
    if (i % 4 == 3) {
      Serial.print(nibble, HEX);
      nibble = 0;
    }
  }
  Serial.print(String("\t ") + count);
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

      // 84-bit wide message
      printBitsAsHex(dataDuringClockChange, 84);
    
    } else if (clockChanges == 161) {

      // regular 80-bit message
      printBitsAsHex(dataDuringClockChange, 80);

    } else {

      // This can happen if the OS does not invoke loop quick enough:
      // Longer delays result in loss of bits and the capture of incomplete messages.
      Serial.print(String("---------- ") + count);

    }

    clockChanges = 0;
    dataDuringClockChangePointer = 0;

  }

}
