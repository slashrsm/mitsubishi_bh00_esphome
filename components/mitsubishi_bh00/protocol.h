#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// Mitsubishi Electric BH00J434B01 ceiling remote (PAR-SL100A-E family).
// Host-testable: this header uses no ESPHome types.
//
// 38 kHz, LSB first. A press is two frames:
//   1. 40-bit marker 23 CB 26 21 04, header about 3510/1700 µs
//   2. 136-bit state, 17 bytes, header about 3450/1730 µs
// Bit mark about 430 µs, one-space about 1285 µs, zero-space about 440 µs.
// A stop mark follows the last bit. Byte 16 is the sum of bytes 0–15, low 8 bits.
//
//   23 CB 26 21 04 | TT | MM | FF | 04 00 00 | XX | 02 26 00 00 | CS
//   0              5    6    7    8          11   12            16
//
// Byte 5 bit 6 is power. Bits 0–5 are the setpoint in half degrees (bit 0 is the
// half). Byte 6 low 3 bits are the mode. Byte 7 low nibble is the fan (bit 0 is
// set on every captured speed, bits 1–2 count 2/3/4, bit 3 marks auto). Byte 7
// high nibble is the vane. There is no fan 1 and no horizontal louver.
// Bytes 8–10 and 12–15 were fixed in every capture. Byte 11 changed on its own.
// The high bits of byte 6 showed up unlabeled; they are kept and sent back.

namespace esphome {
namespace mitsubishi_bh00 {

static constexpr int32_t kMarkerHeaderMark = 3510;
static constexpr int32_t kMarkerHeaderSpace = 1700;
static constexpr int32_t kStateHeaderMark = 3450;
static constexpr int32_t kStateHeaderSpace = 1730;
static constexpr int32_t kBitMark = 430;
static constexpr int32_t kOneSpace = 1285;
static constexpr int32_t kZeroSpace = 440;
// Longer than the receiver idle, so the marker and the state arrive apart.
static constexpr int32_t kInterFrameSpace = 12000;

static constexpr uint8_t kFrameBytes = 17;
static constexpr uint8_t kTempMinX2 = 32;  // 16.0 C
static constexpr uint8_t kTempMaxX2 = 62;  // 31.0 C
static constexpr uint8_t kByte11Default = 0x65;

static constexpr uint8_t kModeFan = 0;
static constexpr uint8_t kModeCool = 1;
static constexpr uint8_t kModeHeat = 2;
static constexpr uint8_t kModeAuto = 3;
static constexpr uint8_t kModeDry = 5;

static constexpr uint8_t kFanAuto = 0;
static constexpr uint8_t kFan2 = 2;
static constexpr uint8_t kFan3 = 3;
static constexpr uint8_t kFan4 = 4;

static constexpr uint8_t kVaneDown = 0;
static constexpr uint8_t kVaneStep3 = 1;
static constexpr uint8_t kVaneStep2 = 2;
static constexpr uint8_t kVaneTop = 3;
static constexpr uint8_t kVaneSwing = 4;
static constexpr uint8_t kVaneStep1 = 5;
static constexpr uint8_t kVaneAll = 6;

enum class DecodeKind : uint8_t { NONE = 0, MARKER = 1, STATE = 2 };

struct Frame {
  bool power_on{false};
  uint8_t temp_x2{48};
  uint8_t mode{kModeCool};
  uint8_t mode_high{0};
  uint8_t fan{kFan4};
  uint8_t vane{kVaneTop};
  uint8_t byte11{kByte11Default};
  uint8_t bytes[kFrameBytes]{};
};

inline bool bh00_mode_ok(uint8_t mode) {
  return mode == kModeFan || mode == kModeCool || mode == kModeHeat || mode == kModeAuto || mode == kModeDry;
}

inline bool bh00_fan_ok(uint8_t fan) { return fan == kFanAuto || fan == kFan2 || fan == kFan3 || fan == kFan4; }

inline bool bh00_vane_ok(uint8_t vane) {
  return vane == kVaneDown || vane == kVaneStep3 || vane == kVaneStep2 || vane == kVaneTop || vane == kVaneSwing ||
         vane == kVaneStep1 || vane == kVaneAll;
}

inline uint8_t bh00_fan_nibble(uint8_t fan) {
  switch (fan) {
    case kFanAuto:
      return 0x09;
    case kFan2:
      return 0x03;
    case kFan3:
      return 0x05;
    case kFan4:
      return 0x07;
    default:
      return 0x09;
  }
}

inline bool bh00_fan_from_nibble(uint8_t low, uint8_t &fan) {
  if ((low & 0x08) != 0) {
    fan = kFanAuto;
    return true;
  }
  switch ((low >> 1) & 0x03) {
    case 1:
      fan = kFan2;
      return true;
    case 2:
      fan = kFan3;
      return true;
    case 3:
      fan = kFan4;
      return true;
    default:
      return false;
  }
}

inline uint8_t bh00_checksum(const uint8_t *bytes) {
  unsigned sum = 0;
  for (int i = 0; i < 16; i++)
    sum += bytes[i];
  return static_cast<uint8_t>(sum);
}

inline bool bh00_encode(const Frame &in, uint8_t out[kFrameBytes]) {
  if (in.temp_x2 > 63 || !bh00_mode_ok(in.mode) || !bh00_fan_ok(in.fan) || !bh00_vane_ok(in.vane))
    return false;
  out[0] = 0x23;
  out[1] = 0xCB;
  out[2] = 0x26;
  out[3] = 0x21;
  out[4] = 0x04;
  out[5] = static_cast<uint8_t>((in.temp_x2 & 0x3F) | (in.power_on ? 0x40 : 0));
  out[6] = static_cast<uint8_t>((in.mode_high & 0xF8) | (in.mode & 0x07));
  out[7] = static_cast<uint8_t>((in.vane << 4) | bh00_fan_nibble(in.fan));
  out[8] = 0x04;
  out[9] = 0x00;
  out[10] = 0x00;
  out[11] = in.byte11;
  out[12] = 0x02;
  out[13] = 0x26;
  out[14] = 0x00;
  out[15] = 0x00;
  out[16] = bh00_checksum(out);
  return true;
}

inline bool bh00_decode_bytes(const uint8_t *bytes, size_t len, Frame &out) {
  if (len != kFrameBytes || bytes[0] != 0x23 || bytes[1] != 0xCB || bytes[2] != 0x26 || bytes[3] != 0x21 ||
      bytes[4] != 0x04 || bh00_checksum(bytes) != bytes[16])
    return false;
  uint8_t fan = 0;
  const uint8_t vane = bytes[7] >> 4;
  if (!bh00_fan_from_nibble(bytes[7] & 0x0F, fan) || !bh00_vane_ok(vane) || !bh00_mode_ok(bytes[6] & 0x07))
    return false;
  out.power_on = (bytes[5] & 0x40) != 0;
  out.temp_x2 = bytes[5] & 0x3F;
  out.mode = bytes[6] & 0x07;
  out.mode_high = bytes[6] & 0xF8;
  out.fan = fan;
  out.vane = vane;
  out.byte11 = bytes[11];
  std::memcpy(out.bytes, bytes, kFrameBytes);
  return true;
}

// Positive values are marks, negative values are spaces, matching ESPHome.
template<typename Emit> void bh00_emit_frame(const uint8_t *bytes, size_t len, int32_t header_mark,
                                             int32_t header_space, Emit &&emit) {
  emit(header_mark);
  emit(-header_space);
  for (size_t i = 0; i < len; i++) {
    for (int bit = 0; bit < 8; bit++) {
      emit(kBitMark);
      emit((bytes[i] & (1u << bit)) != 0 ? -kOneSpace : -kZeroSpace);
    }
  }
  emit(kBitMark);
}

template<typename Emit> void bh00_emit_press(const uint8_t state[kFrameBytes], Emit &&emit) {
  const uint8_t marker[5] = {0x23, 0xCB, 0x26, 0x21, 0x04};
  bh00_emit_frame(marker, 5, kMarkerHeaderMark, kMarkerHeaderSpace, emit);
  emit(-kInterFrameSpace);
  bh00_emit_frame(state, kFrameBytes, kStateHeaderMark, kStateHeaderSpace, emit);
}

inline bool bh00_header_at(const int32_t *raw, size_t count, size_t index) {
  if (index + 1 >= count)
    return false;
  const int32_t mark = raw[index];
  const int32_t space = raw[index + 1];
  if (mark < 2800 || mark > 4500 || space >= 0)
    return false;
  const int32_t gap = -space;
  return gap >= 1400 && gap <= 2200;
}

// Whole bytes, or -1 when the bit count is short. *end is the first pulse not consumed.
inline int bh00_read_bytes(const int32_t *raw, size_t count, size_t index, uint8_t *bytes, size_t max_bytes,
                           size_t *end) {
  index += 2;
  size_t nbits = 0;
  while (index + 1 < count && nbits < max_bytes * 8) {
    const int32_t mark = raw[index];
    const int32_t space = raw[index + 1];
    if (mark < 250 || mark > 700 || space >= 0)
      break;
    const int32_t width = -space;
    if (width < 250 || width > 1800)
      break;
    if (nbits % 8 == 0)
      bytes[nbits / 8] = 0;
    if (width >= 800)
      bytes[nbits / 8] |= static_cast<uint8_t>(1u << (nbits % 8));
    nbits++;
    index += 2;
  }
  *end = index;
  if (nbits == 0 || nbits % 8 != 0)
    return -1;
  return static_cast<int>(nbits / 8);
}

// A buffer may hold the marker, the state, or both. A valid state wins.
inline DecodeKind bh00_decode(const int32_t *raw, size_t count, Frame &out) {
  if (raw == nullptr || count < 2)
    return DecodeKind::NONE;
  DecodeKind found = DecodeKind::NONE;
  size_t index = 0;
  while (index + 1 < count) {
    if (!bh00_header_at(raw, count, index)) {
      index++;
      continue;
    }
    uint8_t bytes[kFrameBytes]{};
    size_t end = index;
    const int nbytes = bh00_read_bytes(raw, count, index, bytes, kFrameBytes, &end);
    if (end <= index) {
      index++;
      continue;
    }
    if (nbytes == 5 && bytes[0] == 0x23 && bytes[1] == 0xCB && bytes[2] == 0x26 && bytes[3] == 0x21 &&
        bytes[4] == 0x04) {
      if (found != DecodeKind::STATE)
        found = DecodeKind::MARKER;
    } else if (nbytes == kFrameBytes && bh00_decode_bytes(bytes, kFrameBytes, out)) {
      found = DecodeKind::STATE;
    }
    index = end;
  }
  return found;
}

}  // namespace mitsubishi_bh00
}  // namespace esphome
