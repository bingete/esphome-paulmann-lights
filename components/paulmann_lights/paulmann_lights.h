#pragma once

#include "esphome/components/esp32_ble_client/ble_client_base.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/number/number.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

#include <deque>
#include <map>

namespace esphome::paulmann_lights {

enum ControlType : uint8_t {
  CONTROL_TYPE_TIMER = 0,
  CONTROL_TYPE_WORKING_MODE = 1,
  CONTROL_TYPE_CONTROLLER_ENABLE = 2,
};

class PaulmannLights;

class PaulmannLightOutput : public light::LightOutput {
 public:
  void set_parent(PaulmannLights *parent) { this->parent_ = parent; }
  void setup_state(light::LightState *state) override;
  void apply_remote_state(bool on, uint8_t brightness, uint16_t color_mireds);

  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;

 protected:
  PaulmannLights *parent_{nullptr};
  light::LightState *light_state_{nullptr};
  bool suppress_write_{false};
};

class PaulmannControlNumber : public number::Number {
 public:
  void set_parent(PaulmannLights *parent) { this->parent_ = parent; }
  void set_control_type(ControlType control_type) { this->control_type_ = control_type; }

 protected:
  void control(float value) override;

  PaulmannLights *parent_{nullptr};
  ControlType control_type_{CONTROL_TYPE_TIMER};
};

class PaulmannLights : public esp32_ble_client::BLEClientBase {
 public:
  void setup() override;
  void dump_config() override;
  bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_name(const std::string &name) { this->name_ = name; }
  void set_password(const std::string &password) { this->password_ = password; }
  void set_connection_retries(uint8_t retries) { this->connection_retries_ = retries; }
  void set_update_interval(uint32_t update_interval) { this->update_interval_ms_ = update_interval; }

  void set_system_id_sensor(text_sensor::TextSensor *sensor) { this->system_id_sensor_ = sensor; }
  void set_model_sensor(text_sensor::TextSensor *sensor) { this->model_sensor_ = sensor; }
  void set_serial_number_sensor(text_sensor::TextSensor *sensor) { this->serial_number_sensor_ = sensor; }
  void set_firmware_revision_sensor(text_sensor::TextSensor *sensor) { this->firmware_revision_sensor_ = sensor; }
  void set_hardware_revision_sensor(text_sensor::TextSensor *sensor) { this->hardware_revision_sensor_ = sensor; }
  void set_software_revision_sensor(text_sensor::TextSensor *sensor) { this->software_revision_sensor_ = sensor; }
  void set_manufacturer_sensor(text_sensor::TextSensor *sensor) { this->manufacturer_sensor_ = sensor; }
  void set_ieee_certification_sensor(text_sensor::TextSensor *sensor) { this->ieee_certification_sensor_ = sensor; }
  void set_pnp_id_sensor(text_sensor::TextSensor *sensor) { this->pnp_id_sensor_ = sensor; }

  void write_onoff(bool on);
  void write_brightness(uint8_t brightness);
  void write_color_temperature(uint16_t color_mireds);
  void write_control(ControlType control_type, uint8_t value);
  void sync_system_time();
  void set_light_output(PaulmannLightOutput *light_output) { this->light_output_ = light_output; }
  void set_timer_number(PaulmannControlNumber *number) { this->timer_number_ = number; }
  void set_working_mode_number(PaulmannControlNumber *number) { this->working_mode_number_ = number; }
  void set_controller_enable_number(PaulmannControlNumber *number) { this->controller_enable_number_ = number; }

 protected:
  struct Handles {
    uint16_t onoff{0};
    uint16_t brightness{0};
    uint16_t color{0};
    uint16_t timer{0};
    uint16_t working_mode{0};
    uint16_t controller_enable{0};
    uint16_t password{0};
    uint16_t system_time{0};

    uint16_t info_system_id{0};
    uint16_t info_model{0};
    uint16_t info_serial_number{0};
    uint16_t info_firmware_revision{0};
    uint16_t info_hardware_revision{0};
    uint16_t info_software_revision{0};
    uint16_t info_manufacturer{0};
    uint16_t info_ieee_certification{0};
    uint16_t info_pnp_id{0};
  } handles_;

  bool authenticated_{false};
  bool auth_write_pending_{false};
  bool read_in_progress_{false};
  uint8_t connection_attempts_{0};
  uint32_t last_connect_attempt_ms_{0};
  uint32_t last_poll_ms_{0};
  uint32_t update_interval_ms_{30000};
  uint8_t connection_retries_{5};
  std::string name_;
  std::string password_{"1234"};

  bool on_{false};
  uint8_t brightness_{100};
  uint16_t color_mireds_{370};
  uint8_t working_mode_{0};

  std::deque<uint16_t> pending_reads_;
  uint16_t current_read_handle_{0};

  text_sensor::TextSensor *system_id_sensor_{nullptr};
  text_sensor::TextSensor *model_sensor_{nullptr};
  text_sensor::TextSensor *serial_number_sensor_{nullptr};
  text_sensor::TextSensor *firmware_revision_sensor_{nullptr};
  text_sensor::TextSensor *hardware_revision_sensor_{nullptr};
  text_sensor::TextSensor *software_revision_sensor_{nullptr};
  text_sensor::TextSensor *manufacturer_sensor_{nullptr};
  text_sensor::TextSensor *ieee_certification_sensor_{nullptr};
  text_sensor::TextSensor *pnp_id_sensor_{nullptr};
  PaulmannControlNumber *timer_number_{nullptr};
  PaulmannControlNumber *working_mode_number_{nullptr};
  PaulmannControlNumber *controller_enable_number_{nullptr};
  PaulmannLightOutput *light_output_{nullptr};

  bool write_bytes_(uint16_t handle, const uint8_t *data, uint16_t len);
  bool read_handle_(uint16_t handle);
  void reset_handles_();
  void discover_handles_();
  void authenticate_();
  void poll_state_();
  void begin_poll_reads_();
  void advance_read_queue_();
  void process_read_value_(uint16_t handle, const uint8_t *value, uint16_t value_len);
  void publish_light_state_();
  bool write_color_temperature_payload_(uint16_t color_mireds);
  static std::string bytes_to_string_(const uint8_t *value, uint16_t value_len);
  static std::string bytes_to_hex_(const uint8_t *value, uint16_t value_len);

  bool pending_color_temperature_write_{false};
  bool waiting_for_color_temperature_mode_ack_{false};
  uint16_t pending_color_mireds_{370};
};

}  // namespace esphome::paulmann_lights

#endif
