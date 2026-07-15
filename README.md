# midea-telemetry

Arduino firmware for capturing telemetry from the diagnostic port on Midea mini-splits, located on the outdoor inverter board. Midea sells a handheld inverter tester that plugs into this port. This project reproduces that tester with a cheap ESP32 microcontroller, so you can log the same data yourself and explore the inner workings of your unit.

> ⚠️ **Safety.** The outdoor unit runs on mains voltage and can retain a dangerous charge after being unplugged. Only plug a connector into the diagnostic port if you know what you are doing. You are responsible for your own hardware and safety.

## The protocol

I reverse-engineered the communication between the inverter tester and the diagnostic port and wrote it up on Medium: [Reverse Engineering Midea's ODU Diagnostic Port](https://medium.com/@florian.mckee/reverse-engineering-mideas-odu-diagnostic-port-af603e159053). The sketches in this repository are based on those findings. Start there if you want to understand the protocol.

## Schematics

t.b.d.

## Sketches

### `firmware.ino` — active tester (emulator)

Source: [firmware.ino](arduino/firmware/firmware.ino)

Emulates Midea's inverter tester: it **drives** the bus, sending diagnostic requests and logging the ODU's responses. This lets you capture telemetry **without owning the inverter tester**.

The set of request messages to send is defined in the `messages` table in `loop()`. Edit it to send different requests. Each request/response pair is printed to serial:

```
req=0xAA000000000000000056, res=0xFFFFFFFFFFFFFFFFFFFF, status=NO_RESPONSE_FROM_ODU
req=0xAA000000000000000056, res=0xFFFFFFFFFFFFFFFFFFFF, status=NO_RESPONSE_FROM_ODU
req=0xAA000000000000000056, res=0xFFFFFFFFFFFFFFFFFFFF, status=NO_RESPONSE_FROM_ODU
req=0xAA000000000000000056, res=0x55006D457671401F03B0, status=OK
req=0xAA010000000000000055, res=0x550128A7B3E8006002DE, status=OK
req=0xAA0200000000FF000055, res=0x55022C2A000000000152, status=OK
req=0xAA030000000000000053, res=0x55030000160EA20000E2, status=OK
req=0xAA000000000000000056, res=0x550400000000002C2C4F, status=OK
req=0xAA010000000000000055, res=0x55054F00000000000057, status=OK
req=0xAA0200000000FF000055, res=0x550600000000000000A5, status=OK
req=0xAA030000000000000053, res=0x55006D457671401F03B0, status=OK
req=0xAA000000000000000056, res=0x550128AAB3EC006002D7, status=OK
req=0xAA010000000000000055, res=0x55022C2B000000000151, status=OK
req=0xAA0200000000FF000055, res=0x55030000160EA70000DD, status=OK
req=0xAA030000000000000053, res=0x550400000000002C2C4F, status=OK
...
req=0xAA010000000000000055, res=0x550128A0B3EC006002E1, status=OK
req=0xAA0200000000FF000055, res=0x55022C2B000000000151, status=OK
req=0xAA030000000000000053, res=0x55030000160BA00000E3, status=CHECKSUM_ERROR
...
```

### `sniffer.ino` — passive sniffer

Source: [sniffer.ino](arduino/sniffer/sniffer.ino)

Passively **listens** on the bus while the **inverter tester is plugged in**, decoding the request/response cycles between the tester and the ODU. Useful for reverse-engineering the protocol. Each request/response pair is printed to serial:

```
req=0xAA010000000000000055, res=0xA00A0000000000000060, status=CHECKSUM_ERROR
req=0xAA010000000000000055, res=0xFFFFFFFFFFFFFFFFFFFF, status=NO_RESPONSE_FROM_ODU
req=0xAA000000000000000056, res=0xFFFFFFFFFFFFFFFFFFFF, status=NO_RESPONSE_FROM_ODU
req=0xAA000000000000000056, res=0x55030000160EAB0000D9, status=OK
req=0xAA010000000000000055, res=0x55040000000000292955, status=OK
req=0xAA0200000000FF000055, res=0x55006C467671431F03AD, status=OK
req=0xAA030000000000000053, res=0x550127AEB3E7006002D9, status=OK
req=                      , res=                      , status=INCOMPLETE
req=0xAA030000000000000053, res=0x55022928000000000157, status=OK
req=0xAA010000000000000055, res=0x55030000160EAA0000DA, status=OK
req=0xAA0200000000FF000055, res=0x55040000000000292955, status=OK
req=0xAA030000000000000053, res=0x55056100000000000045, status=OK
req=0xAA000000000000000056, res=0x550600000000000000A5, status=OK
req=0xAA010000000000000055, res=0x55006C467671431F03AD, status=OK
req=0xAA0200000000FF000055, res=0x550127ABB3E7006002DC, status=OK

...
```

Note: On my ESP32-C3, I regularly see messages that don't decode fully — some bits are lost when loop() isn't called fast enough. There are ways around this, but I prefer to keep the sketch simple, and I can still capture enough data for analysis (even if it takes a couple of tries).

When a request or response fails to decode, you'll see a line like:
```
req=                      , res=                      , status=INCOMPLETE          
```

## Building & flashing

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) and select your board (e.g.,  ESP32C3) via the Boards Manager.
2. Open the sketch you want (`arduino/firmware/firmware.ino` or `arduino/sniffer/sniffer.ino`).
3. Select your board and serial port, then upload.
4. Open the **Serial Monitor at 115200 baud** to view the captured telemetry.

## Status

Early / experimental. The protocol is still being reverse-engineered, and the meaning of individual bytes is not yet fully documented.
