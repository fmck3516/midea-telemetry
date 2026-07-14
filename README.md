# midea-telemetry

Arduino firmware for reading telemetry from the **diagnostic port on Midea mini-splits**, located on the outdoor inverter board. Midea sells a handheld inverter tester that plugs into this port. This project reproduces that tester with a cheap ESP32 microcontroller so you can log the same data yourself and explore the inner workings of the unit.

> ⚠️ **Safety.** The outdoor unit runs on mains voltage and can retain a dangerous charge after being unplugged. Only plug a connector into the diagnostic port if you know what you are doing. You are responsible for your own hardware and safety.

## The protocol

I reverse-engineered the diagnostic port's protocol and wrote it up on Medium: [Reverse Engineering Midea's ODU Diagnostic Port](https://medium.com/@florian.mckee/reverse-engineering-mideas-odu-diagnostic-port-af603e159053). The sketches in this repository are based on those findings. Start there if you want to understand the message format the code implements.

## Schematics

t.b.d.

## Sketches

### `firmware.ino` — active tester (emulator)

Source: [firmware.ino](arduino/firmware/firmware.ino)

Emulates Midea's inverter tester: it **drives** the bus, sending diagnostic requests and logging the ODU's responses. This lets you capture telemetry **without owning the inverter tester**.

The set of request messages to send is defined in the `messages` table in `loop()`. Edit it to send different requests. Each request/response cycle is printed to serial as the request bytes followed by the response bytes, both in hex:

```
 0x5500000000000000006A 0x................
 0x558000000000000000AA 0x................
 ...
```

(An all-`F` response means the ODU did not answer.)

### `sniffer.ino` — passive sniffer

Source: [sniffer.ino](arduino/sniffer/sniffer.ino)

Passively **listens** on the bus while the **inverter tester is plugged in**, decoding the request/response cycles between the tester and the ODU. Useful for reverse-engineering the protocol. Each request/response cycle is printed to serial as the request bytes followed by the response bytes, both in hex:

```
5500000000000000006A	 80
558000000000000000AA	 84
...
```

## Building & flashing

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) and select your board (e.g.,  ESP32C3) via the Boards Manager.
2. Open the sketch you want (`arduino/firmware/firmware.ino` or `arduino/sniffer/sniffer.ino`).
3. Select your board and serial port, then upload.
4. Open the **Serial Monitor at 115200 baud** to view the captured telemetry.

## Status

Early / experimental. The protocol is still being reverse-engineered, and the meaning of individual bytes is not yet fully documented.
