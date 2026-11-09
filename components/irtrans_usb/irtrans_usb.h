#pragma once

#include <string>
#include <vector>

#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#include "irdata.h"

namespace esphome {
namespace irtrans_usb {

enum class IrtransState : uint8_t {
  IDLE = 0,
  HANDSHAKE_SEND,
  HANDSHAKE_WAIT_ACK,
  HANDSHAKE_WAIT_VERSION,
  LEARN_SEND_CMD,
  LEARN_WAIT_ACK,
  LEARN_WAIT_LEN,
  LEARN_WAIT_BODY,
  TRANSMIT_SEND,
  TRANSMIT_WAIT_ACK,
};

class IRTransUSB : public Component, public uart::UARTDevice, public api::CustomAPIDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_learn_timeout(uint32_t timeout_ms) { this->learn_timeout_ms_ = timeout_ms; }
  void set_status_sensor(text_sensor::TextSensor *sensor) { this->status_sensor_ = sensor; }
  void set_last_payload_sensor(text_sensor::TextSensor *sensor) { this->last_payload_sensor_ = sensor; }
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { this->connected_sensor_ = sensor; }

  void api_learn();
  void api_transmit(std::string payload);

 protected:
  void set_status_(const std::string &status);
  void set_connected_(bool connected);
  void flush_rx_();
  bool uart_ready_() const;
  bool write_frame_(const IRData &ird, bool with_sequence);
  void start_handshake_();
  void process_handshake_();
  void process_learn_();
  void process_transmit_();
  void finish_learn_ok_(IRData &ird);
  void fail_(const std::string &message);
  void prepare_send_frame_(IRData &ird);
  bool read_into_buffer_(std::vector<uint8_t> &buf, size_t need);

  text_sensor::TextSensor *status_sensor_{nullptr};
  text_sensor::TextSensor *last_payload_sensor_{nullptr};
  binary_sensor::BinarySensor *connected_sensor_{nullptr};

  uint32_t learn_timeout_ms_{10000};
  IrtransState state_{IrtransState::IDLE};
  uint32_t state_deadline_ms_{0};
  uint8_t io_sequence_{0};
  bool use_sequence_{true};
  bool device_ready_{false};
  uint8_t handshake_attempts_{0};
  uint32_t next_handshake_ms_{0};
  std::string version_;

  IRData work_{};
  uint8_t learn_len_{0};
  std::vector<uint8_t> rx_buf_;
};

}  // namespace irtrans_usb
}  // namespace esphome
