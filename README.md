# Mitsubishi BH00 for ESPHome

External component for the Mitsubishi Electric **BH00J434B01** ceiling remote (PAR-SL100A-E family, Mr Slim and City Multi). It creates one climate entity. It targets **ESPHome 2026.11**.

The frame format is [PROTOCOL.md](PROTOCOL.md). The codec is [`components/mitsubishi_bh00/protocol.h`](components/mitsubishi_bh00/protocol.h). This is not ESPHome's built-in `mitsubishi` climate. That platform is the SG15D wall remote, and this ceiling head ignores it.

## Install

```yaml
external_components:
  - source: github://slashrsm/mitsubishi_bh00_esphome
    components: [mitsubishi_bh00]

remote_transmitter:
  id: ir_tx
  pin: GPIO4
  carrier_duty_percent: 50%

remote_receiver:
  id: ir_rx
  pin:
    number: GPIO14
    inverted: true
    mode:
      input: true
      pullup: true
  tolerance: 55%
  filter: 50us
  idle: 10ms

mitsubishi_bh00:
  name: Mitsubishi BH00
  transmitter_id: ir_tx
  receiver_id: ir_rx
```

Keep Wi-Fi power save off (`wifi: power_save_mode: none`) or the receiver drops edges. [`example/hailege.yaml`](example/hailege.yaml) is a complete config for the Hailege ESP8285 board (TX GPIO4, RX GPIO14). It loads this checkout instead of GitHub:

```bash
cp example/secrets.yaml.example example/secrets.yaml
esphome run example/hailege.yaml
```

## Climate

| Control | Remote |
|---|---|
| Off | Power bit clear. The last mode, setpoint, fan, and vane stay in the frame. |
| Cool, Heat, Heat/Cool, Dry, Fan only | Cool, heat, the four-arrow auto, dehumidify, fan. |
| Temperature | 16.0–31.0 °C in 0.5 °C steps. |
| Fan Auto, 2, 3, 4 | The remote has no fan 1. Auto is the standard fan mode. 2, 3, and 4 are custom fan modes. |
| Preset | The vane: `swing`, `all`, `top`, `step1`, `step2`, `step3`, `down`. `step1` is the first stop below top. |
| IR debug | Diagnostic switch, default off. Logs `TX` and `RX` lines. |

A received frame updates the climate and does not transmit. For 800 ms after a transmit the receiver ignores its own LED. The same state frame inside 300 ms is ignored. Nothing is sent at boot.

The handset cleared the vane nibble when heat was selected. This component keeps the vane you set and sends it in every mode.

## Development

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I components/mitsubishi_bh00 \
  tests/test_protocol.cpp -o /tmp/test_protocol && /tmp/test_protocol
```

GitHub Actions runs that test and `esphome compile example/hailege.yaml` on ESPHome 2026.11.
