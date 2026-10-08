#include "jvs165s.h"

#include <algorithm>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace jvs165s {

static const char *const TAG = "jvs165s";

void JVS165SStopTestButton::press_action() {
  this->parent_->send_stop_test();
}

void JVS165SResumeTestButton::press_action() {
  this->parent_->send_resume_test();
}

void JVS165SSpeed1500Button::press_action() {
  this->parent_->set_speed_1500();
}

void JVS165SSpeed2700Button::press_action() {
  this->parent_->set_speed_2700();
}

void JVS165SSpeed3100Button::press_action() {
  this->parent_->set_speed_3100();
}

void JVS165SComponent::setup() {
  if (this->direction_pin_ != nullptr) {
    this->direction_pin_->setup();
    this->direction_pin_->digital_write(false);  // RX
  }

  this->rx_frame_.reserve(32);

  this->publish_running_(false);
  this->publish_priming_(false);
  this->publish_prime_seconds_(0);

  if (this->commanded_rpm_sensor_ != nullptr) {
    this->commanded_rpm_sensor_->publish_state(this->target_rpm_);
  }

  ESP_LOGI(TAG, "JVS165S controller initialized");
}

void JVS165SComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "JVS165S Pool Pump Controller:");
  ESP_LOGCONFIG(TAG, "  UART: 1200 baud, 8N1");
  ESP_LOGCONFIG(TAG, "  RS485 direction LOW = RX");
  ESP_LOGCONFIG(TAG, "  RS485 direction HIGH = TX");
  ESP_LOGCONFIG(TAG, "  Prime duration: %u seconds", PRIME_DURATION_MS / 1000);
  ESP_LOGCONFIG(TAG, "  Prime command interval: %u ms", PRIME_COMMAND_INTERVAL_MS);
  ESP_LOGCONFIG(TAG, "  Running transaction interval: %u ms", RUN_TRANSACTION_INTERVAL_MS);
  ESP_LOGCONFIG(TAG, "  Default target RPM: %u", DEFAULT_RUN_RPM);
}

void JVS165SComponent::loop() {
  this->process_uart_();

  const uint32_t now = millis();

  if (this->waiting_for_response_ &&
      static_cast<uint32_t>(now - this->response_started_ms_) >= RESPONSE_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Response timeout");
    this->waiting_for_response_ = false;
    this->expected_response_ = ExpectedResponse::NONE;
  }

  if (this->state_ == PumpState::PRIMING) {
    this->update_prime_countdown_();

    if (static_cast<uint32_t>(now - this->prime_started_ms_) >= PRIME_DURATION_MS) {
      this->enter_running_();
      return;
    }

    if (!this->waiting_for_response_ &&
        static_cast<uint32_t>(now - this->last_prime_command_ms_) >= PRIME_COMMAND_INTERVAL_MS) {
      this->send_resume_command_();
    }

    return;
  }

  if (this->state_ != PumpState::RUNNING) {
    return;
  }

  if (this->waiting_for_response_) {
    return;
  }

  if (this->speed_change_pending_) {
    this->speed_change_pending_ = false;
    this->send_rpm_command_(this->target_rpm_);
    this->next_transaction_extended_ = true;
    this->last_run_transaction_ms_ = now;
    return;
  }

  if (static_cast<uint32_t>(now - this->last_run_transaction_ms_) < RUN_TRANSACTION_INTERVAL_MS) {
    return;
  }

  if (this->next_transaction_extended_) {
    this->send_extended_poll_();
    this->next_transaction_extended_ = false;
  } else {
    this->send_rpm_command_(this->target_rpm_);
    this->next_transaction_extended_ = true;
  }

  this->last_run_transaction_ms_ = now;
}

uint8_t JVS165SComponent::crc8_(const uint8_t *data, size_t len) const {
  uint8_t crc = 0x01;

  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];

    for (uint8_t bit = 0; bit < 8; bit++) {
      if ((crc & 0x80) != 0) {
        crc = static_cast<uint8_t>((crc << 1) ^ 0x07);
      } else {
        crc = static_cast<uint8_t>(crc << 1);
      }
    }
  }

  return crc;
}

void JVS165SComponent::send_logical_packet_(
    const std::vector<uint8_t> &payload,
    ExpectedResponse expected) {
  if (payload.empty() || this->direction_pin_ == nullptr) {
    return;
  }

  std::vector<uint8_t> logical(payload);
  logical.push_back(this->crc8_(logical.data(), logical.size()));

  std::vector<uint8_t> wire;
  wire.reserve(logical.size() + 4);

  for (uint8_t byte : logical) {
    wire.push_back(byte);

    if (byte == 0x0D) {
      wire.push_back(0x0D);
    }
  }

  wire.push_back(0x0D);
  wire.push_back(0x0A);

  this->direction_pin_->digital_write(true);  // TX
  delayMicroseconds(200);

  this->write_array(wire.data(), wire.size());
  this->flush();

  // flush() has been measured to return after the final UART byte is sent.
  this->direction_pin_->digital_write(false);  // RX

  this->waiting_for_response_ = true;
  this->expected_response_ = expected;
  this->response_started_ms_ = millis();
}

void JVS165SComponent::send_rpm_command_(uint16_t rpm) {
  std::vector<uint8_t> payload{
      0x01,
      0x01,
      static_cast<uint8_t>(rpm & 0xFF),
      static_cast<uint8_t>((rpm >> 8) & 0xFF),
  };

  ESP_LOGD(TAG, "Sending RPM command: %u", rpm);
  this->send_logical_packet_(payload, ExpectedResponse::STATUS_01_40);
}

void JVS165SComponent::send_extended_poll_() {
  const std::vector<uint8_t> payload{0x01, 0x07, 0x60};

  ESP_LOGD(TAG, "Requesting extended status");
  this->send_logical_packet_(payload, ExpectedResponse::EXTENDED_01_60);
}

void JVS165SComponent::send_resume_command_() {
  const std::vector<uint8_t> payload{0x01, 0x01, 0x7A, 0x4D};

  ESP_LOGD(TAG, "Sending Resume/Prime command");
  this->send_logical_packet_(payload, ExpectedResponse::STATUS_01_40);
  this->last_prime_command_ms_ = millis();
}

void JVS165SComponent::send_stop_test() {
  ESP_LOGI(TAG, "STOP requested");

  this->state_ = PumpState::STOPPED;
  this->stop_pending_ = true;
  this->speed_change_pending_ = false;
  this->next_transaction_extended_ = false;

  this->publish_priming_(false);
  this->publish_prime_seconds_(0);

  if (this->commanded_rpm_sensor_ != nullptr) {
    this->commanded_rpm_sensor_->publish_state(0);
  }

  // STOP is a normal 0-RPM command.
  this->send_rpm_command_(0);
}

void JVS165SComponent::send_resume_test() {
  ESP_LOGI(TAG, "Resume/Prime requested");

  this->state_ = PumpState::PRIMING;
  this->stop_pending_ = false;
  this->speed_change_pending_ = false;
  this->next_transaction_extended_ = false;

  this->prime_started_ms_ = millis();
  this->last_prime_command_ms_ = this->prime_started_ms_ - PRIME_COMMAND_INTERVAL_MS;
  this->last_prime_seconds_ = -1;

  this->publish_priming_(true);
  this->publish_prime_seconds_(PRIME_DURATION_MS / 1000);

  // Send the first Resume command immediately.
  if (!this->waiting_for_response_) {
    this->send_resume_command_();
  }
}

void JVS165SComponent::set_speed_1500() {
  this->select_speed_(1500);
}

void JVS165SComponent::set_speed_2700() {
  this->select_speed_(2700);
}

void JVS165SComponent::set_speed_3100() {
  this->select_speed_(3100);
}

void JVS165SComponent::select_speed_(uint16_t rpm) {
  if (rpm != 1500 && rpm != 2700 && rpm != 3100) {
    return;
  }

  this->target_rpm_ = rpm;

  ESP_LOGI(TAG, "Selected target speed: %u RPM", rpm);

  if (this->commanded_rpm_sensor_ != nullptr) {
    this->commanded_rpm_sensor_->publish_state(rpm);
  }

  if (this->state_ == PumpState::RUNNING) {
    this->speed_change_pending_ = true;
  }
}

void JVS165SComponent::process_uart_() {
  while (this->available() > 0) {
    uint8_t byte = 0;

    if (!this->read_byte(&byte)) {
      break;
    }

    this->consume_rx_byte_(byte);
  }
}

void JVS165SComponent::consume_rx_byte_(uint8_t byte) {
  if (this->pending_cr_) {
    if (byte == 0x0D) {
      // Escaped literal 0D.
      this->rx_frame_.push_back(0x0D);
      this->pending_cr_ = false;
      return;
    }

    if (byte == 0x0A) {
      // Packet terminator. rx_frame_ contains logical bytes including CRC.
      this->pending_cr_ = false;

      if (!this->rx_frame_.empty()) {
        this->handle_frame_(this->rx_frame_);
      }

      this->rx_frame_.clear();
      return;
    }

    // Unexpected sequence. Preserve the pending CR as data and continue.
    this->rx_frame_.push_back(0x0D);
    this->pending_cr_ = false;
  }

  if (byte == 0x0D) {
    this->pending_cr_ = true;
  } else {
    this->rx_frame_.push_back(byte);
  }

  // Protect against a corrupted stream growing without a terminator.
  if (this->rx_frame_.size() > 64) {
    ESP_LOGW(TAG, "RX frame overflow; clearing buffer");
    this->rx_frame_.clear();
    this->pending_cr_ = false;
  }
}

void JVS165SComponent::handle_frame_(const std::vector<uint8_t> &frame) {
  if (frame.size() < 2) {
    return;
  }

  const uint8_t received_crc = frame.back();
  const uint8_t calculated_crc = this->crc8_(frame.data(), frame.size() - 1);

  if (received_crc != calculated_crc) {
    ESP_LOGW(TAG, "CRC mismatch: received 0x%02X, calculated 0x%02X",
             received_crc, calculated_crc);
    return;
  }

  if (frame.size() >= 16 && frame[0] == 0x01 && frame[1] == 0x40) {
    this->handle_status_01_40_(frame);

    if (this->expected_response_ == ExpectedResponse::STATUS_01_40) {
      this->waiting_for_response_ = false;
      this->expected_response_ = ExpectedResponse::NONE;
    }

    return;
  }

  if (frame.size() >= 16 && frame[0] == 0x01 && frame[1] == 0x60) {
    this->handle_extended_01_60_(frame);

    if (this->expected_response_ == ExpectedResponse::EXTENDED_01_60) {
      this->waiting_for_response_ = false;
      this->expected_response_ = ExpectedResponse::NONE;
    }

    return;
  }

  ESP_LOGV(TAG, "Valid unhandled frame type: 0x%02X 0x%02X",
           frame[0], frame[1]);
}

void JVS165SComponent::handle_status_01_40_(const std::vector<uint8_t> &frame) {
  // Logical frame layout (CRC at index 15):
  // 01 40 SS 00 00 00 00 00 00 DD 00 00 00 RPM_LO RPM_HI CRC
  const uint16_t actual_rpm =
      static_cast<uint16_t>(frame[13]) |
      (static_cast<uint16_t>(frame[14]) << 8);

  const uint8_t diagnostic = frame[9];

  if (this->actual_rpm_sensor_ != nullptr) {
    this->actual_rpm_sensor_->publish_state(actual_rpm);
  }

  if (this->diagnostic_sensor_ != nullptr) {
    this->diagnostic_sensor_->publish_state(diagnostic);
  }

  this->publish_running_(actual_rpm > 0);

  ESP_LOGD(TAG, "Status: actual RPM=%u diagnostic=%u",
           actual_rpm, diagnostic);

  // When STOP has been acknowledged by a 0-RPM status response, force the
  // locally published power value to 0 W. The stopped state intentionally
  // does not continue extended-status polling.
  if (this->stop_pending_ && actual_rpm == 0) {
    this->stop_pending_ = false;

    if (this->power_sensor_ != nullptr) {
      this->power_sensor_->publish_state(0);
    }

    ESP_LOGI(TAG, "STOP confirmed; power published as 0 W");
  }
}

void JVS165SComponent::handle_extended_01_60_(
    const std::vector<uint8_t> &frame) {
  // Confirmed power field: bytes 6-7, little endian.
  const uint16_t watts =
      static_cast<uint16_t>(frame[6]) |
      (static_cast<uint16_t>(frame[7]) << 8);

  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->publish_state(watts);
  }

  ESP_LOGD(TAG, "Extended status: power=%u W", watts);
}

void JVS165SComponent::update_prime_countdown_() {
  if (this->state_ != PumpState::PRIMING) {
    return;
  }

  const uint32_t elapsed = millis() - this->prime_started_ms_;
  const uint32_t remaining_ms =
      elapsed >= PRIME_DURATION_MS ? 0 : PRIME_DURATION_MS - elapsed;

  const uint32_t remaining_seconds =
      (remaining_ms + 999) / 1000;

  if (static_cast<int32_t>(remaining_seconds) != this->last_prime_seconds_) {
    this->publish_prime_seconds_(remaining_seconds);
  }
}

void JVS165SComponent::enter_running_() {
  ESP_LOGI(TAG, "Prime complete; switching to %u RPM", this->target_rpm_);

  this->state_ = PumpState::RUNNING;
  this->publish_priming_(false);
  this->publish_prime_seconds_(0);

  this->speed_change_pending_ = false;
  this->next_transaction_extended_ = true;
  this->last_run_transaction_ms_ = millis();

  if (this->commanded_rpm_sensor_ != nullptr) {
    this->commanded_rpm_sensor_->publish_state(this->target_rpm_);
  }

  if (!this->waiting_for_response_) {
    this->send_rpm_command_(this->target_rpm_);
  } else {
    this->speed_change_pending_ = true;
  }
}

void JVS165SComponent::publish_running_(bool running) {
  if (this->running_sensor_ != nullptr) {
    this->running_sensor_->publish_state(running);
  }
}

void JVS165SComponent::publish_priming_(bool priming) {
  if (this->priming_sensor_ != nullptr) {
    this->priming_sensor_->publish_state(priming);
  }
}

void JVS165SComponent::publish_prime_seconds_(uint32_t seconds) {
  this->last_prime_seconds_ = static_cast<int32_t>(seconds);

  if (this->prime_time_remaining_sensor_ != nullptr) {
    this->prime_time_remaining_sensor_->publish_state(seconds);
  }
}

}  // namespace jvs165s
}  // namespace esphome
