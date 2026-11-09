# esphome-irtrans

ESPHome external component that drives an **IRTrans USB** box from an **ESP32-S3** in USB OTG host mode.

Control is **API-only** (no buttons / text inputs to trigger actions):

1. Call **learn** → point a remote at the IRTrans → press a key briefly  
2. Read the portable JSON payload from `text_sensor.irtrans_last_payload`  
3. Store that payload in **your** wiki  
4. Call **transmit** with the same payload to replay (data **and** modulation)

## Hardware

| Item | Notes |
|------|--------|
| ESP32-S3-DevKitC (dual USB-C) | UART port = flash/logs/power; OTG port = USB host |
| IRTrans USB v3.6 | USB ID `0403:FC60` (FTDI) |
| Powered USB hub | **Required** on most DevKit-C boards — OTG usually provides **no 5V VBUS** |

Important ESPHome settings:

- `logger.hardware_uart: UART0` — do **not** use `USB_SERIAL_JTAG` (it steals GPIO19/20 from OTG)
- Power the DevKit from the UART USB port
- Plug IRTrans into a **powered** hub attached to the OTG port

## Install (ESPHome)

```yaml
external_components:
  - source: github://codebfu/esphome-irtrans@master
    components: [irtrans_usb]

api:
  custom_services: true   # required for learn/transmit services

logger:
  hardware_uart: UART0

usb_host:
  enable_hubs: true
  max_transfer_requests: 32

usb_uart:
  - type: ft23xx
    vid: 0x0403
    pid: 0xFC60
    channels:
      - id: irtrans_uart
        baud_rate: 38400
        buffer_size: 1024

irtrans_usb:
  uart_id: irtrans_uart
  learn_timeout: 10s
  status:
    name: IRTrans Status
  last_payload:
    name: IRTrans Last Payload
  connected:
    name: IRTrans Connected
```

A full DevKit-C example lives in [`esphome/irtrans-s3.yaml`](esphome/irtrans-s3.yaml).

## Home Assistant API

Services are registered via ESPHome `CustomAPIDevice` and appear as:

| Service | Arguments | Effect |
|---------|-----------|--------|
| `esphome.<node_name>_irtrans_learn` | _(none)_ | Start IR learn |
| `esphome.<node_name>_irtrans_transmit` | `payload` (string) | Replay a stored payload |

Example (node name `irtrans-s3`):

```yaml
action: esphome.irtrans_s3_irtrans_learn
```

```yaml
action: esphome.irtrans_s3_irtrans_transmit
data:
  payload: '{"v":1,"fmt":"irtrans-irdata","data_b64":"...","freq_khz":38,"mode":0,"ir_length":12}'
```

### Read-only entities

| Entity | Meaning |
|--------|---------|
| `binary_sensor.*_irtrans_connected` | USB IRTrans enumerated / handshake OK |
| `text_sensor.*_irtrans_status` | `idle` / `learning` / `transmitting` / `ok` / `error: …` |
| `text_sensor.*_irtrans_last_payload` | Last learned JSON payload (copy into your wiki) |

## Payload format

```json
{
  "v": 1,
  "fmt": "irtrans-irdata",
  "data_b64": "<base64 of full IRDATA bytes>",
  "freq_khz": 38,
  "mode": 0,
  "ir_length": 12
}
```

- **`data_b64` is the source of truth** — opaque round-trip of the IRTrans `IRDATA` structure (pulse/pause data **and** modulation fields such as `transmit_freq` / `mode`).
- `freq_khz`, `mode`, `ir_length` are derived helpers for wiki readability only; transmit ignores them and uses `data_b64`.

## Wiki workflow

1. Trigger `irtrans_learn`  
2. When `status` becomes `ok`, copy `last_payload` into your wiki next to a human label (`TV power`, `AC cool`, …)  
3. To replay, paste that exact JSON into `irtrans_transmit`  

The ESP does **not** store a command catalogue and does **not** generate a wiki.

## Protocol notes

USB framing follows [asterics/IRTrans-Server](https://github.com/asterics/IRTrans-Server) (`IRDATA`, `HOST_LEARNIR`, `HOST_SEND`, checksum, USB sequence byte). This is a focused subset (learn + send), not a full `irserver` port.

Default UART baud is **38400** (irserver Linux serial default). If your unit needs another rate, change `usb_uart.channels[].baud_rate`.

## License

MIT — see [LICENSE](LICENSE). Protocol structures derived from IRTrans Server sources (IRTrans GmbH / AsTeRICS fork).
