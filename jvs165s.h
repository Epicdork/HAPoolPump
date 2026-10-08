#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace jvs165s {

class JVS165SComponent;

class JVS165SStopTestButton : public button::Button {
 public:
  explicit JVS165SStopTestButton(JVS165SComponent *parent) : parent_(parent) {}
 protected:
  void press_action() override;
  JVS165SComponent *parent_;
};

class JVS165SResumeTestButton : public button::Button {
 public:
  explicit JVS165SResumeTestButton(JVS165SComponent *parent) : parent_(parent) {}
 protected:
  void press_action() override;
  JVS165SComponent *parent_;
};

class JVS165SSpeed1500Button : public button::Button {
 public:
  explicit JVS165SSpeed1500Button(JVS165SComponent *parent) : parent_(parent) {}
 protected:
  void press_action() override;
  JVS165SComponent *parent_;
};

class JVS165SSpeed2700Button : public button::Button {
 public:
  explicit JVS165SSpeed2700Button(JVS165SComponent *parent) : parent_(parent) {}
 protected:
  void press_action() override;
  JVS165SComponent *parent_;
};

class JVS165SSpeed3100Button : public button::Button {
 public:
  explicit JVS165SSpeed3100Button(JVS165SComponent *parent) : parent_(parent) {}
 protected:
  void press_action() override;
  JVS165SComponent *parent_;
};

class JVS165SComponent : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_direction_pin(GPIOPin *pin) { direction_pin_ = pin; }

  void set_running_sensor(binary_sensor::BinarySensor *sensor) { running_sensor_ = sensor; }
  void set_priming_sensor(binary_sensor::BinarySensor *sensor) { priming_sensor_ = sensor; }
  void set_prime_time_remaining_sensor(sensor::Sensor *sensor) { prime_time_remaining_sensor_ = sensor; }
  void set_commanded_rpm_sensor(sensor::Sensor *sensor) { commanded_rpm_sensor_ = sensor; }
  void set_actual_rpm_sensor(sensor::Sensor *sensor) { actual_rpm_sensor_ = sensor; }
  void set_power_sensor(sensor::Sensor *sensor) { power_sensor_ = sensor; }
  void set_diagnostic_sensor(sensor::Sensor *sensor) { diagnostic_sensor_ = sensor; }

  void set_stop_button(JVS165SStopTestButton *button) { stop_button_ = button; }
  void set_resume_button(JVS165SResumeTestButton *button) { resume_button_ = button; }
  void set_speed_1500_button(JVS165SSpeed1500Button *button) { speed_1500_button_ = button; }
  void set_speed_2700_button(JVS165SSpeed2700Button *button) { speed_2700_button_ = button; }
  void set_speed_3100_button(JVS165SSpeed3100Button *button) { speed_3100_button_ = button; }

  void send_stop_test();
  void send_resume_test();
  void set_speed_1500();
  void set_speed_2700();
  void set_speed_3100();

 protected:
  enum class PumpState : uint8_t {
    STOPPED,
    PRIMING,
    RUNNING,
  };

  enum class ExpectedResponse : uint8_t {
    NONE,
    STATUS_01_40,
    EXTENDED_01_60,
  };

  static constexpr uint32_t PRIME_COMMAND_INTERVAL_MS = 1000;
  static constexpr uint32_t PRIME_DURATION_MS = 60000;
  static constexpr uint32_t RUN_TRANSACTION_INTERVAL_MS = 2135;
  static constexpr uint32_t RESPONSE_TIMEOUT_MS = 1000;
  static constexpr uint16_t DEFAULT_RUN_RPM = 2700;

  GPIOPin *direction_pin_{nullptr};

  binary_sensor::BinarySensor *running_sensor_{nullptr};
  binary_sensor::BinarySensor *priming_sensor_{nullptr};
  sensor::Sensor *prime_time_remaining_sensor_{nullptr};
  sensor::Sensor *commanded_rpm_sensor_{nullptr};
  sensor::Sensor *actual_rpm_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *diagnostic_sensor_{nullptr};

  JVS165SStopTestButton *stop_button_{nullptr};
  JVS165SResumeTestButton *resume_button_{nullptr};
  JVS165SSpeed1500Button *speed_1500_button_{nullptr};
  JVS165SSpeed2700Button *speed_2700_button_{nullptr};
  JVS165SSpeed3100Button *speed_3100_button_{nullptr};

  PumpState state_{PumpState::STOPPED};
  ExpectedResponse expected_response_{ExpectedResponse::NONE};

  uint16_t target_rpm_{DEFAULT_RUN_RPM};

  bool waiting_for_response_{false};
  bool stop_pending_{false};
  bool speed_change_pending_{false};
  bool next_transaction_extended_{false};

  uint32_t response_started_ms_{0};
  uint32_t prime_started_ms_{0};
  uint32_t last_prime_command_ms_{0};
  uint32_t last_run_transaction_ms_{0};

  int32_t last_prime_seconds_{-1};

  std::vector<uint8_t> rx_frame_;
  bool pending_cr_{false};

  uint8_t crc8_(const uint8_t *data, size_t len) const;

  void send_logical_packet_(const std::vector<uint8_t> &payload, ExpectedResponse expected);
  void send_rpm_command_(uint16_t rpm);
  void send_extended_poll_();
  void send_resume_command_();

  void process_uart_();
  void consume_rx_byte_(uint8_t byte);
  void handle_frame_(const std::vector<uint8_t> &frame);

  void handle_status_01_40_(const std::vector<uint8_t> &frame);
  void handle_extended_01_60_(const std::vector<uint8_t> &frame);

  void select_speed_(uint16_t rpm);
  void update_prime_countdown_();
  void enter_running_();

  void publish_running_(bool running);
  void publish_priming_(bool priming);
  void publish_prime_seconds_(uint32_t seconds);
};

}  // namespace jvs165s
}  // namespace esphome
