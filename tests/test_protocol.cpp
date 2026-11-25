// Host checks for the BH00 codec. No ESPHome types.
//
// c++ -std=c++17 -Wall -Wextra -Werror -I components/mitsubishi_bh00 tests/test_protocol.cpp -o /tmp/test_protocol && /tmp/test_protocol

#include <cstdio>
#include <cstring>
#include <vector>

#include "protocol.h"

using esphome::mitsubishi_bh00::DecodeKind;
using esphome::mitsubishi_bh00::Frame;
using esphome::mitsubishi_bh00::bh00_decode;
using esphome::mitsubishi_bh00::bh00_decode_bytes;
using esphome::mitsubishi_bh00::bh00_emit_frame;
using esphome::mitsubishi_bh00::bh00_emit_press;
using esphome::mitsubishi_bh00::bh00_encode;
using esphome::mitsubishi_bh00::kFan2;
using esphome::mitsubishi_bh00::kFan3;
using esphome::mitsubishi_bh00::kFan4;
using esphome::mitsubishi_bh00::kFanAuto;
using esphome::mitsubishi_bh00::kFrameBytes;
using esphome::mitsubishi_bh00::kModeAuto;
using esphome::mitsubishi_bh00::kModeCool;
using esphome::mitsubishi_bh00::kModeDry;
using esphome::mitsubishi_bh00::kModeFan;
using esphome::mitsubishi_bh00::kModeHeat;
using esphome::mitsubishi_bh00::kVaneAll;
using esphome::mitsubishi_bh00::kVaneDown;
using esphome::mitsubishi_bh00::kVaneStep1;
using esphome::mitsubishi_bh00::kVaneStep2;
using esphome::mitsubishi_bh00::kVaneStep3;
using esphome::mitsubishi_bh00::kVaneSwing;
using esphome::mitsubishi_bh00::kVaneTop;

namespace {

int g_failures = 0;

void fail(const char *name, const char *detail) {
  std::printf("FAIL %s: %s\n", name, detail);
  g_failures++;
}

void expect_true(const char *name, bool ok, const char *detail) {
  if (!ok)
    fail(name, detail);
}

void expect_eq(const char *name, int got, int want) {
  if (got != want) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "got %d (0x%X) want %d (0x%X)", got, got, want, want);
    fail(name, buf);
  }
}

int parse_hex(const char *text, uint8_t *out, int cap) {
  int n = 0;
  const char *p = text;
  while (*p != '\0') {
    while (*p == ' ')
      p++;
    if (*p == '\0')
      break;
    if (n >= cap)
      return -1;
    unsigned value = 0;
    if (std::sscanf(p, "%2x", &value) != 1)
      return -1;
    out[n++] = static_cast<uint8_t>(value);
    p += 2;
  }
  return n;
}

void expect_frame(const char *name, const Frame &got, bool power, int temp_x2, int mode, int fan, int vane, int mode_high,
                  int byte11) {
  expect_eq(name, got.power_on ? 1 : 0, power ? 1 : 0);
  expect_eq(name, got.temp_x2, temp_x2);
  expect_eq(name, got.mode, mode);
  expect_eq(name, got.fan, fan);
  expect_eq(name, got.vane, vane);
  expect_eq(name, got.mode_high, mode_high);
  expect_eq(name, got.byte11, byte11);
}

void check_bytes(const char *name, bool power, int temp_x2, int mode, int fan, int vane, int mode_high, int byte11,
                 const char *hex) {
  uint8_t want[kFrameBytes];
  const int n = parse_hex(hex, want, kFrameBytes);
  expect_eq(name, n, kFrameBytes);
  if (n != kFrameBytes)
    return;

  Frame in;
  in.power_on = power;
  in.temp_x2 = static_cast<uint8_t>(temp_x2);
  in.mode = static_cast<uint8_t>(mode);
  in.fan = static_cast<uint8_t>(fan);
  in.vane = static_cast<uint8_t>(vane);
  in.mode_high = static_cast<uint8_t>(mode_high);
  in.byte11 = static_cast<uint8_t>(byte11);
  uint8_t got[kFrameBytes];
  if (!bh00_encode(in, got)) {
    fail(name, "encode rejected a captured state");
    return;
  }
  if (std::memcmp(got, want, kFrameBytes) != 0) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "encode mismatch, checksum %02X vs %02X", got[16], want[16]);
    fail(name, buf);
    return;
  }

  Frame decoded;
  if (!bh00_decode_bytes(want, kFrameBytes, decoded)) {
    fail(name, "captured bytes did not decode");
    return;
  }
  expect_frame(name, decoded, power, temp_x2, mode, fan, vane, mode_high, byte11);

  std::vector<int32_t> raw;
  bh00_emit_frame(want, kFrameBytes, esphome::mitsubishi_bh00::kStateHeaderMark,
                  esphome::mitsubishi_bh00::kStateHeaderSpace, [&raw](int32_t pulse) { raw.push_back(pulse); });
  raw.push_back(-10000);
  Frame from_raw;
  const DecodeKind kind = bh00_decode(raw.data(), raw.size(), from_raw);
  expect_eq(name, static_cast<int>(kind), static_cast<int>(DecodeKind::STATE));
  if (kind == DecodeKind::STATE)
    expect_frame(name, from_raw, power, temp_x2, mode, fan, vane, mode_high, byte11);
}

}  // namespace

int main() {
  // Captured 2026-11-25 from BH00J434B01. Temperature is half-degree counts.
  check_bytes("off 24.0 cool fan4 top", false, 48, kModeCool, kFan4, kVaneTop, 0, 0x65,
              "23 CB 26 21 04 30 01 37 04 00 00 65 02 26 00 00 32");
  check_bytes("cool 24.5 fan4 top", true, 49, kModeCool, kFan4, kVaneTop, 0, 0x65,
              "23 CB 26 21 04 71 01 37 04 00 00 65 02 26 00 00 73");
  check_bytes("cool 25.0 fan4 top", true, 50, kModeCool, kFan4, kVaneTop, 0, 0x66,
              "23 CB 26 21 04 72 01 37 04 00 00 66 02 26 00 00 75");
  check_bytes("dehumidify 25.0", true, 50, kModeDry, kFan4, kVaneTop, 0, 0x66,
              "23 CB 26 21 04 72 05 37 04 00 00 66 02 26 00 00 79");
  check_bytes("fan 25.0", true, 50, kModeFan, kFan4, kVaneTop, 0, 0x66,
              "23 CB 26 21 04 72 00 37 04 00 00 66 02 26 00 00 74");
  check_bytes("auto 25.0", true, 50, kModeAuto, kFan4, kVaneTop, 0, 0x66,
              "23 CB 26 21 04 72 03 37 04 00 00 66 02 26 00 00 77");
  check_bytes("heat 20.0 vane down", true, 40, kModeHeat, kFan4, kVaneDown, 0, 0x66,
              "23 CB 26 21 04 68 02 07 04 00 00 66 02 26 00 00 3C");
  check_bytes("fan auto", true, 40, kModeCool, kFanAuto, kVaneTop, 0, 0x66,
              "23 CB 26 21 04 68 01 39 04 00 00 66 02 26 00 00 6D");
  check_bytes("fan 2", true, 40, kModeCool, kFan2, kVaneTop, 0, 0x66,
              "23 CB 26 21 04 68 01 33 04 00 00 66 02 26 00 00 67");
  check_bytes("fan 3", true, 40, kModeCool, kFan3, kVaneTop, 0, 0x66,
              "23 CB 26 21 04 68 01 35 04 00 00 66 02 26 00 00 69");
  check_bytes("fan 4", true, 40, kModeCool, kFan4, kVaneTop, 0, 0x66,
              "23 CB 26 21 04 68 01 37 04 00 00 66 02 26 00 00 6B");
  check_bytes("vane swing", true, 40, kModeCool, kFan2, kVaneSwing, 0, 0x68,
              "23 CB 26 21 04 68 01 43 04 00 00 68 02 26 00 00 79");
  check_bytes("vane all", true, 40, kModeCool, kFan2, kVaneAll, 0, 0x68,
              "23 CB 26 21 04 68 01 63 04 00 00 68 02 26 00 00 99");
  check_bytes("vane top", true, 40, kModeCool, kFan2, kVaneTop, 0, 0x68,
              "23 CB 26 21 04 68 01 33 04 00 00 68 02 26 00 00 69");
  check_bytes("vane step1", true, 40, kModeCool, kFan2, kVaneStep1, 0, 0x68,
              "23 CB 26 21 04 68 01 53 04 00 00 68 02 26 00 00 89");
  check_bytes("vane step2", true, 49, kModeAuto, kFan4, kVaneStep2, 0, 0x65,
              "23 CB 26 21 04 71 03 27 04 00 00 65 02 26 00 00 65");
  check_bytes("vane step3", true, 40, kModeCool, kFan2, kVaneStep3, 0, 0x68,
              "23 CB 26 21 04 68 01 13 04 00 00 68 02 26 00 00 49");
  check_bytes("vane down", true, 40, kModeCool, kFan2, kVaneDown, 0, 0x68,
              "23 CB 26 21 04 68 01 03 04 00 00 68 02 26 00 00 39");
  check_bytes("mode high bits", true, 49, kModeAuto, kFan4, kVaneAll, 0x20, 0x65,
              "23 CB 26 21 04 71 23 67 04 00 00 65 02 26 00 00 C5");

  Frame round;
  round.power_on = true;
  round.temp_x2 = 41;
  round.mode = kModeHeat;
  round.fan = kFan3;
  round.vane = kVaneStep2;
  round.byte11 = 0x68;
  uint8_t encoded[kFrameBytes];
  expect_true("round trip encode", bh00_encode(round, encoded), "encode failed");
  std::vector<int32_t> press;
  bh00_emit_press(encoded, [&press](int32_t pulse) { press.push_back(pulse); });
  Frame from_press;
  expect_eq("press", static_cast<int>(bh00_decode(press.data(), press.size(), from_press)),
            static_cast<int>(DecodeKind::STATE));
  expect_frame("press", from_press, true, 41, kModeHeat, kFan3, kVaneStep2, 0, 0x68);

  std::vector<int32_t> marker;
  const uint8_t marker_bytes[5] = {0x23, 0xCB, 0x26, 0x21, 0x04};
  bh00_emit_frame(marker_bytes, 5, esphome::mitsubishi_bh00::kMarkerHeaderMark,
                  esphome::mitsubishi_bh00::kMarkerHeaderSpace, [&marker](int32_t pulse) { marker.push_back(pulse); });
  Frame ignored{};
  expect_eq("marker only", static_cast<int>(bh00_decode(marker.data(), marker.size(), ignored)),
            static_cast<int>(DecodeKind::MARKER));

  uint8_t broken[kFrameBytes];
  std::memcpy(broken, encoded, kFrameBytes);
  broken[16] ^= 0xFF;
  Frame bad{};
  expect_true("bad checksum", !bh00_decode_bytes(broken, kFrameBytes, bad), "bad checksum was accepted");

  Frame invalid = round;
  invalid.mode = 4;
  expect_true("bad mode", !bh00_encode(invalid, encoded), "mode 4 was encoded");
  invalid = round;
  invalid.fan = 1;
  expect_true("no fan 1", !bh00_encode(invalid, encoded), "fan 1 was encoded");

  const int32_t noise[] = {100, -100, 100, -400, 400};
  expect_eq("noise", static_cast<int>(bh00_decode(noise, 5, ignored)), static_cast<int>(DecodeKind::NONE));
  expect_eq("empty", static_cast<int>(bh00_decode(nullptr, 0, ignored)), static_cast<int>(DecodeKind::NONE));

  if (g_failures == 0)
    std::printf("ok\n");
  else
    std::printf("%d failure(s)\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
