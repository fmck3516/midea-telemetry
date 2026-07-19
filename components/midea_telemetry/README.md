# `midea_telemetry` ESPHome component

An [ESPHome external component](https://esphome.io/components/external_components/) that emulates Midea's inverter tester on the ODU diagnostic port and exposes all currently known telemetry fields as Home Assistant sensors.

It drives the two-wire bus exactly like [inverter-tester-emulator.ino](../../arduino/inverter-tester-emulator/inverter-tester-emulator.ino) — same wake-up sequence, request table, and timing — but from a dedicated FreeRTOS task, so ESPHome's main loop (Wi-Fi, API) is never blocked by the ~380 ms bit-banged frame cycles. The bus is polled continuously; `update_interval` only controls how often the latest values are published. If the ODU stops answering for 60 s, the sensors report "unavailable" instead of freezing on the last value.

Hardware is the same as for the Arduino sketches (ESP32 + level shifter, see the [schematic](../../schematics/)). ESP32 only — the component needs FreeRTOS and a second core.

## Example configuration

```yaml
esphome:
  name: midea-odu

esp32:
  board: seeed_xiao_esp32s3
  framework:
    type: arduino

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
logger:
ota:
  platform: esphome

external_components:
  - source: github://fmck3516/midea-telemetry   # or, for a local checkout:
    components: [midea_telemetry]                     # - source: components

midea_telemetry:
  clk_pin: GPIO3   # D2 on the XIAO ESP32S3
  dat_pin: GPIO2   # D1
  update_interval: 10s

sensor:
  - platform: midea_telemetry
    indoor_ambient_temperature:
      name: Indoor ambient temperature
    indoor_coil_temperature:
      name: Indoor coil temperature
    outdoor_ambient_temperature:
      name: Outdoor ambient temperature
    outdoor_coil_temperature:
      name: Outdoor coil temperature
    discharge_temperature:
      name: Compressor discharge temperature
    operating_mode:
      name: Operating mode
    compressor_frequency_target:
      name: Compressor frequency (target)
    compressor_frequency_actual:
      name: Compressor frequency (actual)
    outdoor_fan_speed:
      name: Outdoor fan speed
    eev_steps:
      name: EEV opening steps
    indoor_setpoint:
      name: Indoor set-point
    input_voltage:
      name: Input voltage
    current_draw:
      name: Current draw
```

Every sensor is optional — list only the ones you want as entities.

## Fields

Byte mapping and conversion formulas as documented in [Reverse Engineering Midea's ODU Diagnostic Port](https://medium.com/@florian.mckee/reverse-engineering-mideas-odu-diagnostic-port-af603e159053); they match `tools/live_values.py` and the dashboard.

| Sensor | Unit | Response type | Bytes |
|---|---|---|---|
| `indoor_ambient_temperature` | °C | `0x00` | 2 (NTC, Beta model) |
| `indoor_coil_temperature` | °C | `0x00` | 3 (NTC, Beta model) |
| `outdoor_ambient_temperature` | °C | `0x00` | 5 (NTC, Beta model) |
| `outdoor_coil_temperature` | °C | `0x00` | 4 (NTC, Beta model) |
| `discharge_temperature` | °C | `0x00` | 6 (NTC, Steinhart-Hart) |
| `operating_mode` | raw code | `0x02` | 8 |
| `compressor_frequency_target` | Hz | `0x02` | 2 |
| `compressor_frequency_actual` | Hz | `0x02` | 3 |
| `outdoor_fan_speed` | raw | `0x00` | 7+8 (uint16) |
| `eev_steps` | raw | `0x01` | 5+6 (uint16) |
| `indoor_setpoint` | °C | `0x01` | 7 (tentative mapping) |
| `input_voltage` | V | `0x01` | 3 |
| `current_draw` | A | `0x01` | 2 |
