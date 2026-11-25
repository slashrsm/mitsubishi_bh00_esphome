# BH00J434B01

Captured from the handheld ceiling remote. The codec and the host test lock these bytes in: [`components/mitsubishi_bh00/protocol.h`](components/mitsubishi_bh00/protocol.h), [`tests/test_protocol.cpp`](tests/test_protocol.cpp).

38 kHz, LSB first. Bit mark about 430 µs, one-space about 1285 µs, zero-space about 440 µs. Each press sends two frames:

1. A 40-bit marker. Header about 3510 / 1700 µs. Payload `23 CB 26 21 04`.
2. A 136-bit state frame, 17 bytes. Header about 3450 / 1730 µs. The same five bytes, then the fields below. Byte 16 is the sum of bytes 0–15, low 8 bits. A stop mark follows the last bit.

```
23 CB 26 21 04 | TT | MM | FF | 04 00 00 | XX | 02 26 00 00 | CS
0              5    6    7    8          11   12            16
```

| Byte | Field |
|---|---|
| 5 | Bit 6 is power. Bits 0–5 are the setpoint times 2. Bit 0 is the half degree. Off clears bit 6 and leaves the setpoint. |
| 6 | Mode in the low 3 bits: fan `0`, cool `1`, heat `2`, auto `3`, dehumidify `5`. High bits are kept and sent back. They are not named. |
| 7 | Fan in the low nibble, vane in the high nibble. |
| 8–10 | `04 00 00` on this remote. |
| 11 | Changed across presses with no button tied to it. The component stores the last value and sends it back. A fresh device uses `65`. |
| 12–15 | `02 26 00 00` on this remote. |

Fan low nibble, with bit 0 set the way the remote sends it: auto `9`, fan 2 `3`, fan 3 `5`, fan 4 `7`. There is no fan 1.

Vane high nibble, top to down: top `3`, step 1 `5`, step 2 `2`, step 3 `1`, down `0`. Swing is `4`. All positions (circle icon) is `6`. The louver button blinks N/A and adds no field.

Bytes 3–4 stayed `21 04`. The published `MITSUBISHI136` layout is a different remote: whole-degree temperature in byte 6, a complemented checksum, and no 40-bit marker.
