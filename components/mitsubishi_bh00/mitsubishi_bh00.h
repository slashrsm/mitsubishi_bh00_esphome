#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/preferences.h"
#include "protocol.h"

namespace esphome {
namespace mitsubishi_bh00 {

class MitsubishiBh00Climate;

class MitsubishiBh00DebugSwitch final : public switch_::Switch, public Parented<MitsubishiBh00Climate> {
 protected:
  void write_state(bool state) override;
};

class MitsubishiBh00Climate final : public climate_ir::ClimateIR {
 public:
  MitsubishiBh00Climate();

  void setup() override;
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  void set_debug_switch(MitsubishiBh00DebugSwitch *sw) { this->debug_switch_ = sw; }
  void set_debug(bool on) { this->debug_ = on; }

 protected:
  void control(const climate::ClimateCall &call) override;
  void transmit_state() override;

  void apply_frame_(const Frame &frame);
  bool frame_from_state_(Frame &frame) const;
  void remember_();
  void log_frame_(const char *dir, const Frame &frame) const;
  uint8_t fan_from_state_() const;
  uint8_t vane_from_state_() const;

  MitsubishiBh00DebugSwitch *debug_switch_{nullptr};
  bool debug_{false};

  uint8_t operating_mode_{kModeCool};
  uint8_t mode_high_{0};
  uint8_t byte11_{kByte11Default};

  uint32_t tx_at_{0};
  bool tx_guard_{false};
  uint32_t last_rx_ms_{0};
  uint32_t last_rx_hash_{0};
  bool have_rx_hash_{false};

  ESPPreferenceObject pref_{};
};

}  // namespace mitsubishi_bh00
}  // namespace esphome
