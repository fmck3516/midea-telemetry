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
req=0xAA000000000000000056, res=0xFFFFFFFFFFFFFFFFFFFF
req=0xAA000000000000000056, res=0xFFFFFFFFFFFFFFFFFFFF
req=0xAA000000000000000056, res=0xFFFFFFFFFFFFFFFFFFFF
req=0xAA000000000000000056, res=0x550103B7B3DC00600001
req=0xAA010000000000000055, res=0x550200000000000003A6
req=0xAA0200000000FF000055, res=0x55030000160EB70000CD
req=0xAA030000000000000053, res=0x550400000000000000A7
req=0xAA000000000000000056, res=0x550500000000000000A6
req=0xAA010000000000000055, res=0x550600000000000000A5
req=0xAA0200000000FF000055, res=0x5500706E6F70210000CD
req=0xAA030000000000000053, res=0x550103B7B3DC00600001
req=0xAA000000000000000056, res=0x550200000000000003A6
req=0xAA010000000000000055, res=0x55030000160EB70000CD
req=0xAA0200000000FF000055, res=0x550400000000000000A7
req=0xAA030000000000000053, res=0x550500000000000000A6
req=0xAA000000000000000056, res=0x550600000000000000A5
req=0xAA010000000000000055, res=0x5500706E6F70210000CD
req=0xAA0200000000FF000055, res=0x550103B7B3DC00600001
req=0xAA030000000000000053, res=0x550600000000400000A5 // checksum error
...
```

An all-`F` response means the ODU did not answer. Lines that end with `// checksum error` indicate that the checksum validation failed for the response message.

### `sniffer.ino` — passive sniffer

Source: [sniffer.ino](arduino/sniffer/sniffer.ino)

Passively **listens** on the bus while the **inverter tester is plugged in**, decoding the request/response cycles between the tester and the ODU. Useful for reverse-engineering the protocol. Each request/response pair is printed to serial:

```
req=0x5500000000000000006A, res=0xFFFFFFFFFFFFFFFFFFFF
req=0x5500000000000000006A, res=0xFFFFFFFFFFFFFFFFFFFF
req=0x5500000000000000006A, res=0xFFFFFFFFFFFFFFFFFFFF
req=0x5500000000000000006A, res=0xAA6000000000000000A5
req=0x558000000000000000AA, res=0xAA00F636B6CE940000E3
req=0x554000000000FF0000AA, res=0xAA80C05DCD3B0006007F
req=                      , res=                       
...
```

Note: On my ESP32-C3, I regularly see messages that don't decode fully — some bits are lost when loop() isn't called fast enough. There are ways around this, but I prefer to keep the sketch simple, and I can still capture enough data for analysis (even if it takes a couple of tries).

When a request or response fails to decode, you'll see a line like:
```
req=                      , res=                       
```

## Building & flashing

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) and select your board (e.g.,  ESP32C3) via the Boards Manager.
2. Open the sketch you want (`arduino/firmware/firmware.ino` or `arduino/sniffer/sniffer.ino`).
3. Select your board and serial port, then upload.
4. Open the **Serial Monitor at 115200 baud** to view the captured telemetry.

## Status

Early / experimental. The protocol is still being reverse-engineered, and the meaning of individual bytes is not yet fully documented.
