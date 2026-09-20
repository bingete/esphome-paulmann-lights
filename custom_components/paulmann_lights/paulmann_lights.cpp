#include "paulmann_lights.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cmath>
#include <ctime>

namespace esphome::paulmann_lights {

static const char *const TAG = "paulmann_lights";

static const uint16_t UUID_SERVICE_PAULMANN = 0xFFB0;
static const uint16_t UUID_SERVICE_DEVICE_INFO = 0x180A;

static const uint16_t UUID_SYSTEM_TIME = 0xFFB3;
static const uint16_t UUID_TIMER = 0xFFB4;
static const uint16_t UUID_COLOR = 0xFFB6;
static const uint16_t UUID_ONOFF = 0xFFB7;
static const uint16_t UUID_BRIGHTNESS = 0xFFB8;
static const uint16_t UUID_WORKING_MODE = 0xFFB9;
static const uint16_t UUID_PWD = 0xFFBA;
static const uint16_t UUID_CONTROLLER_ENABLE = 0xFFBB;

static const uint16_t UUID_INFO_SYSTEM_ID = 0x2A23;
static const uint16_t UUID_INFO_MODEL = 0x2A24;
static const uint16_t UUID_INFO_SERIAL_NUMBER = 0x2A25;
static const uint16_t UUID_INFO_FIRMWARE_REVISION = 0x2A26;
static const uint16_t UUID_INFO_HARDWARE_REVISION = 0x2A27;
static const uint16_t UUID_INFO_SOFTWARE_REVISION = 0x2A28;
static const uint16_t UUID_INFO_MANUFACTURER = 0x2A29;
static const uint16_t UUID_INFO_IEEE_CERT = 0x2A2A;
static const uint16_t UUID_INFO_PNP_ID = 0x2A50;

void PaulmannLights::setup() {
  ble_client::BLEClient::setup();
  this->set_interval("poll", this->update_interval_ms_, [this]() { this->poll_state_(); });
}

void PaulmannLights::publish_light_state_() {
  if (this->light_output_ != nullptr) {
    this->light_output_->apply_remote_state(this->on_, this->brightness_, this->color_mireds_);
  }
}

void PaulmannLightOutput::setup_state(light::LightState *state) {
  light::LightOutput::setup_state(state);
  this->light_state_ = state;
}

void PaulmannLightOutput::apply_remote_state(bool on, uint8_t brightness, uint16_t color_mireds) {
  if (this->light_state_ == nullptr) {
    return;
  }

  this->suppress_write_ = true;
  auto call = this->light_state_->make_call();
  call.set_state(on);
  call.set_brightness(static_cast<float>(brightness) / 100.0f);
  call.set_color_temperature(static_cast<float>(color_mireds));
  call.set_save(false);
  call.perform();
  this->suppress_write_ = false;
}

void PaulmannLights::dump_config() {
  ESP_LOGCONFIG(TAG, "Paulmann Lights (%s)", this->name_.c_str());
  ble_client::BLEClient::dump_config();
  ESP_LOGCONFIG(TAG, "  Password configured: %s", YESNO(!this->password_.empty()));
  ESP_LOGCONFIG(TAG, "  Connection retries: %u", this->connection_retries_);
  ESP_LOGCONFIG(TAG, "  Poll interval: %u ms", this->update_interval_ms_);
}

bool PaulmannLights::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  if (!ble_client::BLEClient::gattc_event_handler(event, gattc_if, param)) {
    return false;
  }

  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "[%s] BLE connected", this->address_str());
        this->connection_attempts_ = 0;
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
    case ESP_GATTC_CLOSE_EVT: {
      ESP_LOGW(TAG, "[%s] BLE disconnected", this->address_str());
      this->authenticated_ = false;
      this->auth_write_pending_ = false;
      this->read_in_progress_ = false;
      this->pending_reads_.clear();
      this->current_read_handle_ = 0;
      this->reset_handles_();
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->discover_handles_();
      this->authenticate_();
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.handle == this->handles_.password) {
        this->auth_write_pending_ = false;
        this->authenticated_ = param->write.status == ESP_GATT_OK;
        if (this->authenticated_) {
          ESP_LOGI(TAG, "[%s] Authentication successful", this->address_str());
          this->sync_system_time();
          this->poll_state_();
        } else {
          ESP_LOGW(TAG, "[%s] Authentication failed (status=%d)", this->address_str(), param->write.status);
        }
      }
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.handle == this->current_read_handle_) {
        this->read_in_progress_ = false;
        this->current_read_handle_ = 0;
      }

      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] Read failed for handle 0x%04X (status=%d)", this->address_str(), param->read.handle,
                 param->read.status);
      } else {
        this->process_read_value_(param->read.handle, param->read.value, param->read.value_len);
      }

      this->advance_read_queue_();
      break;
    }
    default:
      break;
  }

  return true;
}

void PaulmannLights::write_onoff(bool on) {
  this->on_ = on;
  const uint8_t value = on ? 0x01 : 0x00;
  if (!this->write_bytes_(this->handles_.onoff, &value, 1)) {
    ESP_LOGW(TAG, "[%s] Failed to write on/off state", this->address_str());
  }
}

void PaulmannLights::write_brightness(uint8_t brightness) {
  this->brightness_ = std::min<uint8_t>(100, brightness);
  const uint8_t value = this->brightness_;
  if (!this->write_bytes_(this->handles_.brightness, &value, 1)) {
    ESP_LOGW(TAG, "[%s] Failed to write brightness", this->address_str());
  }
}

void PaulmannLights::write_color_temperature(uint16_t color_mireds) {
  this->color_mireds_ = std::max<uint16_t>(153, std::min<uint16_t>(370, color_mireds));
  const uint8_t payload[2] = {
      static_cast<uint8_t>(this->color_mireds_ & 0xFF),
      static_cast<uint8_t>((this->color_mireds_ >> 8) & 0xFF),
  };
  if (!this->write_bytes_(this->handles_.color, payload, sizeof(payload))) {
    ESP_LOGW(TAG, "[%s] Failed to write color temperature", this->address_str());
  }
}

void PaulmannLights::write_control(ControlType control_type, uint8_t value) {
  uint16_t handle = 0;
  switch (control_type) {
    case CONTROL_TYPE_TIMER:
      handle = this->handles_.timer;
      break;
    case CONTROL_TYPE_WORKING_MODE:
      handle = this->handles_.working_mode;
      break;
    case CONTROL_TYPE_CONTROLLER_ENABLE:
      handle = this->handles_.controller_enable;
      break;
    default:
      break;
  }

  if (!this->write_bytes_(handle, &value, 1)) {
    ESP_LOGW(TAG, "[%s] Failed to write control value type=%u", this->address_str(), control_type);
  }
}

void PaulmannLights::sync_system_time() {
  if (this->handles_.system_time == 0) {
    return;
  }

  const auto now = static_cast<uint32_t>(::time(nullptr));
  if (now == 0) {
    return;
  }

  const uint8_t payload[4] = {
      static_cast<uint8_t>(now & 0xFF),
      static_cast<uint8_t>((now >> 8) & 0xFF),
      static_cast<uint8_t>((now >> 16) & 0xFF),
      static_cast<uint8_t>((now >> 24) & 0xFF),
  };
  this->write_bytes_(this->handles_.system_time, payload, sizeof(payload));
}

bool PaulmannLights::write_bytes_(uint16_t handle, const uint8_t *data, uint16_t len) {
  if (!this->authenticated_ && handle != this->handles_.password) {
    ESP_LOGW(TAG, "[%s] Not authenticated, skipping write", this->address_str());
    return false;
  }
  if (handle == 0) {
    ESP_LOGW(TAG, "[%s] Missing characteristic handle", this->address_str());
    return false;
  }
  if (!this->connected()) {
    ESP_LOGW(TAG, "[%s] Not connected", this->address_str());
    return false;
  }

  auto status = esp_ble_gattc_write_char(this->get_gattc_if(), this->get_conn_id(), handle, len, const_cast<uint8_t *>(data),
                                         ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "[%s] Write failed for handle 0x%04X (status=%d)", this->address_str(), handle, status);
    return false;
  }
  return true;
}

bool PaulmannLights::read_handle_(uint16_t handle) {
  if (!this->authenticated_ || handle == 0 || !this->connected()) {
    return false;
  }

  auto status = esp_ble_gattc_read_char(this->get_gattc_if(), this->get_conn_id(), handle, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "[%s] Read request failed for handle 0x%04X (status=%d)", this->address_str(), handle, status);
    return false;
  }

  this->read_in_progress_ = true;
  this->current_read_handle_ = handle;
  return true;
}

void PaulmannLights::reset_handles_() { this->handles_ = Handles{}; }

void PaulmannLights::discover_handles_() {
  this->reset_handles_();

  auto *onoff = this->get_characteristic(UUID_SERVICE_PAULMANN, UUID_ONOFF);
  auto *brightness = this->get_characteristic(UUID_SERVICE_PAULMANN, UUID_BRIGHTNESS);
  auto *color = this->get_characteristic(UUID_SERVICE_PAULMANN, UUID_COLOR);
  auto *timer = this->get_characteristic(UUID_SERVICE_PAULMANN, UUID_TIMER);
  auto *working_mode = this->get_characteristic(UUID_SERVICE_PAULMANN, UUID_WORKING_MODE);
  auto *controller_enable = this->get_characteristic(UUID_SERVICE_PAULMANN, UUID_CONTROLLER_ENABLE);
  auto *password = this->get_characteristic(UUID_SERVICE_PAULMANN, UUID_PWD);
  auto *system_time = this->get_characteristic(UUID_SERVICE_PAULMANN, UUID_SYSTEM_TIME);

  auto *info_system_id = this->get_characteristic(UUID_SERVICE_DEVICE_INFO, UUID_INFO_SYSTEM_ID);
  auto *info_model = this->get_characteristic(UUID_SERVICE_DEVICE_INFO, UUID_INFO_MODEL);
  auto *info_serial_number = this->get_characteristic(UUID_SERVICE_DEVICE_INFO, UUID_INFO_SERIAL_NUMBER);
  auto *info_firmware_revision = this->get_characteristic(UUID_SERVICE_DEVICE_INFO, UUID_INFO_FIRMWARE_REVISION);
  auto *info_hardware_revision = this->get_characteristic(UUID_SERVICE_DEVICE_INFO, UUID_INFO_HARDWARE_REVISION);
  auto *info_software_revision = this->get_characteristic(UUID_SERVICE_DEVICE_INFO, UUID_INFO_SOFTWARE_REVISION);
  auto *info_manufacturer = this->get_characteristic(UUID_SERVICE_DEVICE_INFO, UUID_INFO_MANUFACTURER);
  auto *info_ieee_certification = this->get_characteristic(UUID_SERVICE_DEVICE_INFO, UUID_INFO_IEEE_CERT);
  auto *info_pnp_id = this->get_characteristic(UUID_SERVICE_DEVICE_INFO, UUID_INFO_PNP_ID);

  this->handles_.onoff = onoff == nullptr ? 0 : onoff->handle;
  this->handles_.brightness = brightness == nullptr ? 0 : brightness->handle;
  this->handles_.color = color == nullptr ? 0 : color->handle;
  this->handles_.timer = timer == nullptr ? 0 : timer->handle;
  this->handles_.working_mode = working_mode == nullptr ? 0 : working_mode->handle;
  this->handles_.controller_enable = controller_enable == nullptr ? 0 : controller_enable->handle;
  this->handles_.password = password == nullptr ? 0 : password->handle;
  this->handles_.system_time = system_time == nullptr ? 0 : system_time->handle;

  this->handles_.info_system_id = info_system_id == nullptr ? 0 : info_system_id->handle;
  this->handles_.info_model = info_model == nullptr ? 0 : info_model->handle;
  this->handles_.info_serial_number = info_serial_number == nullptr ? 0 : info_serial_number->handle;
  this->handles_.info_firmware_revision = info_firmware_revision == nullptr ? 0 : info_firmware_revision->handle;
  this->handles_.info_hardware_revision = info_hardware_revision == nullptr ? 0 : info_hardware_revision->handle;
  this->handles_.info_software_revision = info_software_revision == nullptr ? 0 : info_software_revision->handle;
  this->handles_.info_manufacturer = info_manufacturer == nullptr ? 0 : info_manufacturer->handle;
  this->handles_.info_ieee_certification = info_ieee_certification == nullptr ? 0 : info_ieee_certification->handle;
  this->handles_.info_pnp_id = info_pnp_id == nullptr ? 0 : info_pnp_id->handle;
}

void PaulmannLights::authenticate_() {
  this->authenticated_ = false;
  if (this->handles_.password == 0) {
    ESP_LOGE(TAG, "[%s] Password characteristic not found", this->address_str());
    return;
  }

  if (this->password_.empty()) {
    ESP_LOGE(TAG, "[%s] Password is empty", this->address_str());
    return;
  }

  if (this->write_bytes_(this->handles_.password, reinterpret_cast<const uint8_t *>(this->password_.data()),
                         this->password_.size())) {
    this->auth_write_pending_ = true;
    ESP_LOGI(TAG, "[%s] Authentication command sent, waiting for response", this->address_str());
  }
}

void PaulmannLights::poll_state_() {
  if (!this->connected()) {
    const uint32_t now = millis();
    if (this->connection_attempts_ < this->connection_retries_ && now - this->last_connect_attempt_ms_ > 5000) {
      this->last_connect_attempt_ms_ = now;
      this->connection_attempts_++;
      ESP_LOGW(TAG, "[%s] Reconnecting attempt %u/%u", this->address_str(), this->connection_attempts_,
               this->connection_retries_);
      this->connect();
    }
    return;
  }

  if (this->auth_write_pending_) {
    return;
  }

  if (!this->authenticated_) {
    this->authenticate_();
    return;
  }

  const uint32_t now = millis();
  if (now - this->last_poll_ms_ < this->update_interval_ms_) {
    return;
  }

  this->last_poll_ms_ = now;
  this->begin_poll_reads_();
}

void PaulmannLights::begin_poll_reads_() {
  if (this->read_in_progress_) {
    return;
  }

  this->pending_reads_.clear();

  const uint16_t state_handles[] = {
      this->handles_.onoff,
      this->handles_.brightness,
      this->handles_.color,
      this->handles_.timer,
      this->handles_.working_mode,
      this->handles_.controller_enable,
  };

  for (auto handle : state_handles) {
    if (handle != 0) {
      this->pending_reads_.push_back(handle);
    }
  }

  if (this->system_id_sensor_ != nullptr && this->handles_.info_system_id != 0) {
    this->pending_reads_.push_back(this->handles_.info_system_id);
  }
  if (this->model_sensor_ != nullptr && this->handles_.info_model != 0) {
    this->pending_reads_.push_back(this->handles_.info_model);
  }
  if (this->serial_number_sensor_ != nullptr && this->handles_.info_serial_number != 0) {
    this->pending_reads_.push_back(this->handles_.info_serial_number);
  }
  if (this->firmware_revision_sensor_ != nullptr && this->handles_.info_firmware_revision != 0) {
    this->pending_reads_.push_back(this->handles_.info_firmware_revision);
  }
  if (this->hardware_revision_sensor_ != nullptr && this->handles_.info_hardware_revision != 0) {
    this->pending_reads_.push_back(this->handles_.info_hardware_revision);
  }
  if (this->software_revision_sensor_ != nullptr && this->handles_.info_software_revision != 0) {
    this->pending_reads_.push_back(this->handles_.info_software_revision);
  }
  if (this->manufacturer_sensor_ != nullptr && this->handles_.info_manufacturer != 0) {
    this->pending_reads_.push_back(this->handles_.info_manufacturer);
  }
  if (this->ieee_certification_sensor_ != nullptr && this->handles_.info_ieee_certification != 0) {
    this->pending_reads_.push_back(this->handles_.info_ieee_certification);
  }
  if (this->pnp_id_sensor_ != nullptr && this->handles_.info_pnp_id != 0) {
    this->pending_reads_.push_back(this->handles_.info_pnp_id);
  }

  this->advance_read_queue_();
}

void PaulmannLights::advance_read_queue_() {
  if (this->read_in_progress_) {
    return;
  }

  while (!this->pending_reads_.empty()) {
    const uint16_t handle = this->pending_reads_.front();
    this->pending_reads_.pop_front();
    if (this->read_handle_(handle)) {
      return;
    }
  }
}

void PaulmannLights::process_read_value_(uint16_t handle, const uint8_t *value, uint16_t value_len) {
  if (handle == this->handles_.onoff && value_len > 0) {
    this->on_ = value[0] == 1;
    this->publish_light_state_();
    return;
  }

  if (handle == this->handles_.brightness && value_len > 0) {
    this->brightness_ = std::min<uint8_t>(100, value[0]);
    this->publish_light_state_();
    return;
  }

  if (handle == this->handles_.color && value_len >= 2) {
    this->color_mireds_ = static_cast<uint16_t>(value[0] | (value[1] << 8));
    this->publish_light_state_();
    return;
  }
  if (handle == this->handles_.timer && value_len > 0) {
    if (this->timer_number_ != nullptr) {
      this->timer_number_->publish_state(value[0]);
    }
    return;
  }
  if (handle == this->handles_.working_mode && value_len > 0) {
    if (this->working_mode_number_ != nullptr) {
      this->working_mode_number_->publish_state(value[0]);
    }
    return;
  }
  if (handle == this->handles_.controller_enable && value_len > 0) {
    if (this->controller_enable_number_ != nullptr) {
      this->controller_enable_number_->publish_state(value[0] != 0 ? 1 : 0);
    }
    return;
  }

  if (handle == this->handles_.info_system_id && this->system_id_sensor_ != nullptr) {
    this->system_id_sensor_->publish_state(bytes_to_hex_(value, value_len));
    return;
  }
  if (handle == this->handles_.info_model && this->model_sensor_ != nullptr) {
    this->model_sensor_->publish_state(bytes_to_string_(value, value_len));
    return;
  }
  if (handle == this->handles_.info_serial_number && this->serial_number_sensor_ != nullptr) {
    this->serial_number_sensor_->publish_state(bytes_to_string_(value, value_len));
    return;
  }
  if (handle == this->handles_.info_firmware_revision && this->firmware_revision_sensor_ != nullptr) {
    this->firmware_revision_sensor_->publish_state(bytes_to_string_(value, value_len));
    return;
  }
  if (handle == this->handles_.info_hardware_revision && this->hardware_revision_sensor_ != nullptr) {
    this->hardware_revision_sensor_->publish_state(bytes_to_string_(value, value_len));
    return;
  }
  if (handle == this->handles_.info_software_revision && this->software_revision_sensor_ != nullptr) {
    this->software_revision_sensor_->publish_state(bytes_to_string_(value, value_len));
    return;
  }
  if (handle == this->handles_.info_manufacturer && this->manufacturer_sensor_ != nullptr) {
    this->manufacturer_sensor_->publish_state(bytes_to_string_(value, value_len));
    return;
  }
  if (handle == this->handles_.info_ieee_certification && this->ieee_certification_sensor_ != nullptr) {
    this->ieee_certification_sensor_->publish_state(bytes_to_hex_(value, value_len));
    return;
  }
  if (handle == this->handles_.info_pnp_id && this->pnp_id_sensor_ != nullptr) {
    this->pnp_id_sensor_->publish_state(bytes_to_hex_(value, value_len));
    return;
  }
}

std::string PaulmannLights::bytes_to_string_(const uint8_t *value, uint16_t value_len) {
  std::string str(reinterpret_cast<const char *>(value), value_len);
  str.erase(std::find(str.begin(), str.end(), '\0'), str.end());
  return str;
}

std::string PaulmannLights::bytes_to_hex_(const uint8_t *value, uint16_t value_len) {
  char buffer[format_hex_size(64)];
  const auto copy_len = std::min<uint16_t>(value_len, 64);
  return std::string(format_hex_pretty_to(buffer, value, copy_len, ':'));
}

light::LightTraits PaulmannLightOutput::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
  traits.set_min_mireds(153.0f);
  traits.set_max_mireds(370.0f);
  return traits;
}

void PaulmannLightOutput::write_state(light::LightState *state) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (this->suppress_write_) {
    return;
  }

  bool on = false;
  float brightness = 1.0f;
  float color_temperature = 370.0f;
  state->current_values_as_binary(&on);
  state->current_values_as_brightness(&brightness);
  state->current_values_as_ct(&color_temperature, &brightness);

  this->parent_->write_onoff(on);
  this->parent_->write_brightness(static_cast<uint8_t>(roundf(brightness * 100.0f)));
  this->parent_->write_color_temperature(static_cast<uint16_t>(roundf(color_temperature)));
}

void PaulmannControlNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }

  const auto rounded = static_cast<uint8_t>(roundf(value));
  this->parent_->write_control(this->control_type_, rounded);
  this->publish_state(rounded);
}

}  // namespace esphome::paulmann_lights

#endif
