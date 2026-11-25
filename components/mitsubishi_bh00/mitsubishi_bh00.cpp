#include "mitsubishi_bh00.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace mitsubishi_bh00 {

static constexpr uint32_t PREF_HASH = 0xB400E501;
static constexpr uint32_t CARRIER_HZ = 38000;
static constexpr uint32_t ECHO_IGNORE_MS = 800;
static constexpr uint32_t REPEAT_WINDOW_MS = 300;

static const char *const kLabelFan2 = "2";
static const char *const kLabelFan3 = "3";
static const char *const kLabelFan4 = "4";
static const char *const kFans[] = {kLabelFan2, kLabelFan3, kLabelFan4};

static const char *const kLabelSwing = "swing";
static const char *const kLabelAll = "all";
static const char *const kLabelTop = "top";
static const char *const kLabelStep1 = "step1";
static const char *const kLabelStep2 = "step2";
static const char *const kLabelStep3 = "step3";
static const char *const kLabelDown = "down";
static const char *const kVanes[] = {kLabelSwing, kLabelAll,   kLabelTop,  kLabelStep1,
                                     kLabelStep2, kLabelStep3, kLabelDown};

ESPHOME_LOG_TAG(TAG, "mitsubishi_bh00");

struct SavedExtra {
  uint8_t mode;
  uint8_t mode_high;
  uint8_t byte11;
} __attribute__((packed));

static const char *mode_name(uint8_t mode) {
  switch (mode) {
    case kModeFan:
      return "fan";
    case kModeCool:
      return "cool";
    case kModeHeat:
      return "heat";
    case kModeAuto:
      return "auto";
    case kModeDry:
      return "dehumidify";
    default:
      return nullptr;
  }
}

static const char *fan_name(uint8_t fan) {
  switch (fan) {
    case kFanAuto:
      return "auto";
    case kFan2:
      return kLabelFan2;
    case kFan3:
      return kLabelFan3;
    case kFan4:
      return kLabelFan4;
    default:
      return nullptr;
  }
}

static const char *vane_name(uint8_t vane) {
  switch (vane) {
    case kVaneSwing:
      return kLabelSwing;
    case kVaneAll:
      return kLabelAll;
    case kVaneTop:
      return kLabelTop;
    case kVaneStep1:
      return kLabelStep1;
    case kVaneStep2:
      return kLabelStep2;
    case kVaneStep3:
      return kLabelStep3;
    case kVaneDown:
      return kLabelDown;
    default:
      return nullptr;
  }
}

static uint8_t mode_from_climate(climate::ClimateMode mode) {
  switch (mode) {
    case climate::CLIMATE_MODE_FAN_ONLY:
      return kModeFan;
    case climate::CLIMATE_MODE_HEAT:
      return kModeHeat;
    case climate::CLIMATE_MODE_HEAT_COOL:
      return kModeAuto;
    case climate::CLIMATE_MODE_DRY:
      return kModeDry;
    case climate::CLIMATE_MODE_COOL:
    default:
      return kModeCool;
  }
}

static climate::ClimateMode climate_from_mode(uint8_t mode) {
  switch (mode) {
    case kModeFan:
      return climate::CLIMATE_MODE_FAN_ONLY;
    case kModeHeat:
      return climate::CLIMATE_MODE_HEAT;
    case kModeAuto:
      return climate::CLIMATE_MODE_HEAT_COOL;
    case kModeDry:
      return climate::CLIMATE_MODE_DRY;
    case kModeCool:
    default:
      return climate::CLIMATE_MODE_COOL;
  }
}

static bool same_text(StringRef ref, const char *text) {
  if (text == nullptr || ref.size() != std::strlen(text))
    return false;
  return std::memcmp(ref.c_str(), text, ref.size()) == 0;
}

static void format_hex(const uint8_t *bytes, size_t len, char *out, size_t out_len) {
  size_t pos = 0;
  out[0] = '\0';
  for (size_t i = 0; i < len && pos + 4 < out_len; i++)
    pos += static_cast<size_t>(std::snprintf(out + pos, out_len - pos, i == 0 ? "%02X" : " %02X", bytes[i]));
}

MitsubishiBh00Climate::MitsubishiBh00Climate()
    : climate_ir::ClimateIR(16.0f, 31.0f, 0.5f, true, true, {climate::CLIMATE_FAN_AUTO}, {}, {}) {
  this->set_supported_custom_fan_modes(kFans);
  this->set_supported_custom_presets(kVanes);
}

void MitsubishiBh00DebugSwitch::write_state(bool state) {
  auto *parent = this->get_parent();
  if (parent == nullptr)
    return;
  parent->set_debug(state);
  this->publish_state(state);
}

void MitsubishiBh00Climate::setup() {
  this->disable_loop();
  this->pref_ = global_preferences->make_preference<SavedExtra>(PREF_HASH, true);
  SavedExtra saved{};
  if (this->pref_.load(&saved) && bh00_mode_ok(saved.mode) && (saved.mode_high & 0x07) == 0) {
    this->operating_mode_ = saved.mode;
    this->mode_high_ = saved.mode_high;
    this->byte11_ = saved.byte11;
  }

  climate_ir::ClimateIR::setup();
  if (!this->has_custom_preset())
    this->set_custom_preset_(kLabelTop);
  // Off keeps the operating mode loaded above. Any other restored mode replaces it.
  if (this->mode != climate::CLIMATE_MODE_OFF)
    this->operating_mode_ = mode_from_climate(this->mode);
  if (std::isnan(this->target_temperature))
    this->target_temperature = 24;
  if (this->debug_switch_ != nullptr)
    this->debug_switch_->restore();
  this->publish_state();
}

void MitsubishiBh00Climate::dump_config() {
  climate_ir::ClimateIR::dump_config();
  ESP_LOGCONFIG(TAG, "  BH00 vane is the preset list");
}

void MitsubishiBh00Climate::remember_() {
  SavedExtra saved;
  saved.mode = this->operating_mode_;
  saved.mode_high = this->mode_high_;
  saved.byte11 = this->byte11_;
  this->pref_.save(&saved);
}

uint8_t MitsubishiBh00Climate::fan_from_state_() const {
  if (this->has_custom_fan_mode()) {
    const StringRef fan = this->get_custom_fan_mode();
    if (same_text(fan, kLabelFan2))
      return kFan2;
    if (same_text(fan, kLabelFan3))
      return kFan3;
    if (same_text(fan, kLabelFan4))
      return kFan4;
  }
  return kFanAuto;
}

uint8_t MitsubishiBh00Climate::vane_from_state_() const {
  if (!this->has_custom_preset())
    return kVaneTop;
  const StringRef vane = this->get_custom_preset();
  if (same_text(vane, kLabelSwing))
    return kVaneSwing;
  if (same_text(vane, kLabelAll))
    return kVaneAll;
  if (same_text(vane, kLabelStep1))
    return kVaneStep1;
  if (same_text(vane, kLabelStep2))
    return kVaneStep2;
  if (same_text(vane, kLabelStep3))
    return kVaneStep3;
  if (same_text(vane, kLabelDown))
    return kVaneDown;
  return kVaneTop;
}

bool MitsubishiBh00Climate::frame_from_state_(Frame &frame) const {
  int half = static_cast<int>(lroundf(this->target_temperature * 2.0f));
  if (half < kTempMinX2)
    half = kTempMinX2;
  if (half > kTempMaxX2)
    half = kTempMaxX2;
  frame.power_on = this->mode != climate::CLIMATE_MODE_OFF;
  frame.temp_x2 = static_cast<uint8_t>(half);
  frame.mode = frame.power_on ? mode_from_climate(this->mode) : this->operating_mode_;
  frame.mode_high = this->mode_high_;
  frame.fan = this->fan_from_state_();
  frame.vane = this->vane_from_state_();
  frame.byte11 = this->byte11_;
  return bh00_encode(frame, frame.bytes);
}

void MitsubishiBh00Climate::log_frame_(const char *dir, const Frame &frame) const {
  if (!this->debug_)
    return;
  char hex[80];
  format_hex(frame.bytes, kFrameBytes, hex, sizeof(hex));
  const char *mode = mode_name(frame.mode);
  const char *fan = fan_name(frame.fan);
  const char *vane = vane_name(frame.vane);
  char flags[16] = "";
  if (frame.mode_high != 0)
    std::snprintf(flags, sizeof(flags), " b6=%02X", static_cast<unsigned>(frame.mode_high | frame.mode));
  ESP_LOGI(TAG, "%s %s %u.%uC mode=%s fan=%s vane=%s b11=%02X%s | %s", dir, frame.power_on ? "on" : "off",
           frame.temp_x2 / 2, (frame.temp_x2 & 1) != 0 ? 5 : 0, mode != nullptr ? mode : "?",
           fan != nullptr ? fan : "?", vane != nullptr ? vane : "?", frame.byte11, flags, hex);
}

void MitsubishiBh00Climate::transmit_state() {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "No transmitter configured");
    return;
  }
  int half = static_cast<int>(lroundf(this->target_temperature * 2.0f));
  if (half < kTempMinX2)
    half = kTempMinX2;
  if (half > kTempMaxX2)
    half = kTempMaxX2;
  this->target_temperature = static_cast<float>(half) / 2.0f;

  Frame frame;
  if (!this->frame_from_state_(frame)) {
    ESP_LOGW(TAG, "Refusing to send an incomplete state");
    return;
  }
  if (frame.power_on)
    this->operating_mode_ = frame.mode;
  this->remember_();
  this->log_frame_("TX", frame);

  this->tx_at_ = millis();
  this->tx_guard_ = true;
  auto call = this->transmitter_->transmit();
  auto *data = call.get_data();
  data->set_carrier_frequency(CARRIER_HZ);
  bh00_emit_press(frame.bytes, [data](int32_t pulse) {
    if (pulse >= 0)
      data->mark(static_cast<uint32_t>(pulse));
    else
      data->space(static_cast<uint32_t>(-pulse));
  });
  call.set_send_times(1);
  call.perform();
}

void MitsubishiBh00Climate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
    if (this->mode != climate::CLIMATE_MODE_OFF)
      this->operating_mode_ = mode_from_climate(this->mode);
  }
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();
  if (call.has_custom_fan_mode())
    this->set_custom_fan_mode_(call.get_custom_fan_mode());
  else if (call.get_fan_mode().has_value())
    this->set_fan_mode_(*call.get_fan_mode());
  if (call.has_custom_preset())
    this->set_custom_preset_(call.get_custom_preset());
  this->transmit_state();
  this->publish_state();
}

void MitsubishiBh00Climate::apply_frame_(const Frame &frame) {
  this->operating_mode_ = frame.mode;
  this->mode_high_ = frame.mode_high;
  this->byte11_ = frame.byte11;
  this->mode = frame.power_on ? climate_from_mode(frame.mode) : climate::CLIMATE_MODE_OFF;
  this->target_temperature = static_cast<float>(frame.temp_x2) / 2.0f;
  if (frame.fan == kFanAuto)
    this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
  else
    this->set_custom_fan_mode_(fan_name(frame.fan));
  this->set_custom_preset_(vane_name(frame.vane));
  this->remember_();
  this->publish_state();
}

bool MitsubishiBh00Climate::on_receive(remote_base::RemoteReceiveData data) {
  const auto &raw = data.get_raw_data();
  if (raw.empty())
    return false;
  if (this->tx_guard_ && millis() - this->tx_at_ < ECHO_IGNORE_MS)
    return false;

  Frame frame;
  if (bh00_decode(raw.data(), raw.size(), frame) != DecodeKind::STATE)
    return false;

  uint32_t hash = 2166136261u;
  for (uint8_t byte : frame.bytes)
    hash = (hash ^ byte) * 16777619u;
  const uint32_t now = millis();
  if (this->have_rx_hash_ && hash == this->last_rx_hash_ && now - this->last_rx_ms_ < REPEAT_WINDOW_MS)
    return false;
  this->have_rx_hash_ = true;
  this->last_rx_hash_ = hash;
  this->last_rx_ms_ = now;

  this->log_frame_("RX", frame);
  this->apply_frame_(frame);
  return false;
}

}  // namespace mitsubishi_bh00
}  // namespace esphome
