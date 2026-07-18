#include <Arduino.h>

#define PIN_CLK D2
#define PIN_DAT D1

#define MESSAGE_WIDTH 80
#define FRAME_BYTES (MESSAGE_WIDTH / 8)
#define NUM_RESPONSES 7

// Bytes 0-8 of each response frame, indexed by the request's second byte
// (0xAA 0x00 .. -> slot 0, 0xAA 0x01 .. -> slot 1, ...). Byte 9, the
// checksum, is generated automatically whenever a frame is sent, so any
// payload byte can be changed freely. Edit the defaults here, or change them
// at runtime over USB serial without reflashing (type "help" in the monitor).
uint8_t responsePayloads[NUM_RESPONSES][FRAME_BYTES - 1] = {
  {0x55, 0x00, 0x6D, 0x45, 0x76, 0x71, 0x40, 0x1F, 0x03},
  {0x55, 0x01, 0x28, 0xA7, 0xB3, 0xE8, 0x00, 0x60, 0x02},
  {0x55, 0x02, 0x2C, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x01},
  {0x55, 0x03, 0x00, 0x00, 0x16, 0x0E, 0xA2, 0x00, 0x00},
  {0x55, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 0x2C},
  {0x55, 0x05, 0x4F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x55, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

// A request can arrive as 80 bits, or 84 with the 4-bit handshake prefix.
uint32_t requestBits[84];
uint32_t bitCount = 0;
uint32_t lastEdge = 0;
int clockLevel;

char commandLine[64];
size_t commandLength = 0;

// Releasing to INPUT_PULLUP (instead of plain INPUT) means the sketch also
// works on the bench against inverter-tester-emulator without the bus or
// external pull-up resistors.
static inline void setLine(uint8_t pin, uint8_t level) {
  if (level)
    pinMode(pin, INPUT_PULLUP);  // release - pull-up brings it high
  else {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
}

// The last byte makes all ten bytes of a frame sum to zero modulo 256.
uint8_t checksumFor(const uint8_t *payload) {
  uint8_t sum = 0;
  for (size_t i = 0; i < FRAME_BYTES - 1; i++) {
    sum += payload[i];
  }
  return (uint8_t)(0 - sum);
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

bool checksumValid(const uint32_t *bits) {
  uint8_t sum = 0;
  for (size_t byteIndex = 0; byteIndex < FRAME_BYTES; byteIndex++) {
    sum += byteFromBits(bits, byteIndex);
  }
  return sum == 0;
}

void printBitsAsHex(const uint32_t *bits) {
  for (size_t byteIndex = 0; byteIndex < FRAME_BYTES; byteIndex++) {
    uint8_t b = byteFromBits(bits, byteIndex);
    if (b < 0x10) HWCDCSerial.print('0');  // keep the leading zero
    HWCDCSerial.print(b, HEX);
  }
}

void printBytesAsHex(const uint8_t *bytes, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (bytes[i] < 0x10) HWCDCSerial.print('0');  // keep the leading zero
    HWCDCSerial.print(bytes[i], HEX);
  }
}

static bool waitForClock(uint8_t level, uint32_t timeoutMs) {
  uint32_t start = millis();
  while (digitalRead(PIN_CLK) != level) {
    if (millis() - start > timeoutMs) return false;
  }
  return true;
}

// Drives one response frame onto the bus. The tester keeps driving the
// clock: it raises CLK, samples DAT ~1ms later, then drives CLK low again.
// So each bit is placed on DAT while CLK is low and advanced on the falling
// edge. The full frame (payload + fresh checksum) is copied to frameOut for
// logging. Returns false if the tester stops clocking mid-frame.
bool sendResponse(const uint8_t *payload, uint8_t *frameOut) {
  memcpy(frameOut, payload, FRAME_BYTES - 1);
  frameOut[FRAME_BYTES - 1] = checksumFor(payload);
  bool ok = true;
  for (uint32_t i = 0; i < MESSAGE_WIDTH && ok; i++) {
    setLine(PIN_DAT, (frameOut[i / 8] >> (i % 8)) & 1);  // LSB-first
    ok = waitForClock(HIGH, 2000) && waitForClock(LOW, 2000);
  }
  pinMode(PIN_DAT, INPUT_PULLUP);  // release the data line
  return ok;
}

void handleRequest(const uint32_t *bits) {
  uint8_t header = byteFromBits(bits, 0);
  uint8_t index = byteFromBits(bits, 1);
  uint8_t frame[FRAME_BYTES];
  bool responded = false;
  const char *status;

  if (header != 0xAA) {
    status = "NOT_A_REQUEST";
  } else if (!checksumValid(bits)) {
    status = "CHECKSUM_ERROR";
  } else if (index >= NUM_RESPONSES) {
    status = "NO_RESPONSE_CONFIGURED";
  } else {
    // The tester starts clocking the response ~60ms after the request, so
    // respond first and log afterwards.
    responded = sendResponse(responsePayloads[index], frame);
    status = responded ? "OK" : "TIMEOUT";
  }

  HWCDCSerial.print("req=0x");
  printBitsAsHex(bits);
  HWCDCSerial.print(", res=0x");
  if (responded) {
    printBytesAsHex(frame, FRAME_BYTES);
  } else {
    HWCDCSerial.print("--------------------");
  }
  HWCDCSerial.print(", status=");
  HWCDCSerial.println(status);
}

void printFrame(size_t slot) {
  HWCDCSerial.print(slot);
  HWCDCSerial.print(": 0x");
  printBytesAsHex(responsePayloads[slot], FRAME_BYTES - 1);
  uint8_t checksum = checksumFor(responsePayloads[slot]);
  printBytesAsHex(&checksum, 1);
  HWCDCSerial.println();
}

void printAllFrames() {
  for (size_t slot = 0; slot < NUM_RESPONSES; slot++) {
    printFrame(slot);
  }
}

void printHelp() {
  HWCDCSerial.println("commands:");
  HWCDCSerial.println("  show                    print all response frames (checksum included)");
  HWCDCSerial.println("  set <slot> <18 hex>     replace bytes 0-8, e.g. set 2 55022C2A0000000001");
  HWCDCSerial.println("                          (20 hex chars also accepted; the checksum is ignored)");
  HWCDCSerial.println("  poke <slot> <byte> <value>  change one byte (0-8); value is decimal,");
  HWCDCSerial.println("                          or hex with 0x prefix, e.g. poke 2 2 0x2B");
}

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool parseHexBytes(const char *s, uint8_t *out, size_t count) {
  for (size_t i = 0; i < count; i++) {
    int hi = hexNibble(s[2 * i]);
    int lo = hexNibble(s[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (hi << 4) | lo;
  }
  return true;
}

// Parses a byte value: hex with a 0x prefix (0x2B), decimal otherwise (43).
// Returns -1 if the value is malformed or out of range.
static int parseByteValue(const char *s) {
  bool isHex = s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
  const char *digits = isHex ? s + 2 : s;
  char *end;
  long value = strtol(digits, &end, isHex ? 16 : 10);
  if (end == digits || *end != 0 || value < 0 || value > 255) return -1;
  return (int)value;
}

void handleCommand(char *line) {
  char *verb = strtok(line, " ");
  if (verb == NULL) return;

  if (strcmp(verb, "show") == 0) {
    printAllFrames();
    return;
  }

  if (strcmp(verb, "set") == 0) {
    char *slotArg = strtok(NULL, " ");
    char *hexArg = strtok(NULL, " ");
    if (slotArg == NULL || hexArg == NULL) {
      HWCDCSerial.println("err: usage: set <slot> <18 hex chars>");
      return;
    }
    int slot = atoi(slotArg);
    size_t hexLength = strlen(hexArg);
    uint8_t payload[FRAME_BYTES - 1];
    if (slot < 0 || slot >= NUM_RESPONSES) {
      HWCDCSerial.println("err: slot must be 0-6");
    } else if ((hexLength != 18 && hexLength != 20) || !parseHexBytes(hexArg, payload, FRAME_BYTES - 1)) {
      HWCDCSerial.println("err: expected 18 hex chars (bytes 0-8)");
    } else {
      memcpy(responsePayloads[slot], payload, FRAME_BYTES - 1);
      printFrame(slot);
    }
    return;
  }

  if (strcmp(verb, "poke") == 0) {
    char *slotArg = strtok(NULL, " ");
    char *posArg = strtok(NULL, " ");
    char *valueArg = strtok(NULL, " ");
    if (slotArg == NULL || posArg == NULL || valueArg == NULL) {
      HWCDCSerial.println("err: usage: poke <slot> <byte 0-8> <value>");
      return;
    }
    int slot = atoi(slotArg);
    int pos = atoi(posArg);
    int value = parseByteValue(valueArg);
    if (slot < 0 || slot >= NUM_RESPONSES) {
      HWCDCSerial.println("err: slot must be 0-6");
    } else if (pos < 0 || pos > FRAME_BYTES - 2) {
      HWCDCSerial.println("err: byte position must be 0-8");
    } else if (value < 0) {
      HWCDCSerial.println("err: value must be 0-255 (decimal) or 0x00-0xFF (hex)");
    } else {
      responsePayloads[slot][pos] = (uint8_t)value;
      printFrame(slot);
    }
    return;
  }

  printHelp();
}

void pollSerial() {
  while (HWCDCSerial.available()) {
    char c = HWCDCSerial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      commandLine[commandLength] = 0;
      commandLength = 0;
      handleCommand(commandLine);
    } else if (commandLength < sizeof(commandLine) - 1) {
      commandLine[commandLength++] = c;
    }
  }
}

void setup() {
  HWCDCSerial.begin();
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DAT, INPUT_PULLUP);
  delay(2000);
  clockLevel = digitalRead(PIN_CLK);
  lastEdge = millis();
  HWCDCSerial.println("ODU emulator ready, current response frames:");
  printAllFrames();
  printHelp();
}

void loop() {

  pollSerial();

  int newClockLevel = digitalRead(PIN_CLK);

  if (newClockLevel != clockLevel) {

    clockLevel = newClockLevel;
    lastEdge = millis();

    // The tester puts the data bit on the line before driving the clock low,
    // so the rising edge is the middle of the bit - the safest place to sample.
    if (newClockLevel == HIGH && bitCount < sizeof(requestBits) / sizeof(requestBits[0])) {
      requestBits[bitCount++] = digitalRead(PIN_DAT);
    }

  } else if (bitCount > 0 && millis() - lastEdge > 10) {

    // No clock edge for 10ms -> the message is complete. The tester waits
    // ~60ms before clocking the response, so there is time to get ready.
    if (bitCount == 80) {

      handleRequest(requestBits);

    } else if (bitCount == 84) {

      // 4 zero bits from the handshake + 80-bit wide message
      handleRequest(requestBits + 4);

    } else if (bitCount > 4) {

      HWCDCSerial.println("req=                      , res=                      , status=INCOMPLETE");

    }
    // 4 bits or fewer: handshake fragments or noise -> discard silently

    bitCount = 0;
    clockLevel = digitalRead(PIN_CLK);
    lastEdge = millis();

  }

}
