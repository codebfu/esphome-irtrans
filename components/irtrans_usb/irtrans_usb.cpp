#include "irtrans_usb.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/log.h"

#include "payload.h"

namespace esphome {
namespace irtrans_usb {

static const char *const TAG = "irtrans_usb";

void IRTransUSB::setup() {
  this->register_service(&IRTransUSB::api_learn, "irtrans_learn");
  this->register_service(&IRTransUSB::api_transmit, "irtrans_transmit", {"payload"});
  this->set_status_("idle");
  this->set_connected_(false);
  this->start_handshake_();
}

void IRTransUSB::dump_config() {
  ESP_LOGCONFIG(TAG, "IRTrans USB");
  ESP_LOGCONFIG(TAG, "  Learn timeout: %u ms", this->learn_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Sequence mode: %s", YESNO(this->use_sequence_));
}

void IRTransUSB::loop() {
  if (!this->uart_ready_()) {
    if (this->device_ready_) {
      this->device_ready_ = false;
      this->set_connected_(false);
      this->set_status_("disconnected");
      this->state_ = IrtransState::IDLE;
    }
    if (this->state_ == IrtransState::IDLE && millis() >= this->next_handshake_ms_) {
      this->start_handshake_();
    }
    return;
  }

  switch (this->state_) {
    case IrtransState::HANDSHAKE_SEND:
    case IrtransState::HANDSHAKE_WAIT_ACK:
    case IrtransState::HANDSHAKE_WAIT_VERSION:
      this->process_handshake_();
      break;
    case IrtransState::LEARN_SEND_CMD:
    case IrtransState::LEARN_WAIT_ACK:
    case IrtransState::LEARN_WAIT_LEN:
    case IrtransState::LEARN_WAIT_BODY:
      this->process_learn_();
      break;
    case IrtransState::TRANSMIT_SEND:
    case IrtransState::TRANSMIT_WAIT_ACK:
      this->process_transmit_();
      break;
    case IrtransState::IDLE:
    default:
      break;
  }
}

void IRTransUSB::api_learn() {
  if (!this->uart_ready_()) {
    this->set_status_("error: uart not connected");
    return;
  }
  if (this->state_ != IrtransState::IDLE || !this->device_ready_) {
    this->set_status_("error: busy");
    return;
  }

  memset(&this->work_, 0, sizeof(this->work_));
  this->work_.command = HOST_LEARNIR;
  this->work_.len = 7;
  this->work_.address = ADRESS_LOCAL;
  this->work_.target_mask = 0;
  this->work_.ir_length = 0;
  this->work_.checksumme = irdata_checksum(&this->work_);

  this->learn_len_ = 0;
  this->rx_buf_.clear();
  this->flush_rx_();
  this->state_ = IrtransState::LEARN_SEND_CMD;
  this->set_status_("learning");
  ESP_LOGI(TAG, "Learn started (timeout %u ms)", this->learn_timeout_ms_);
}

void IRTransUSB::api_transmit(std::string payload) {
  if (!this->uart_ready_()) {
    this->set_status_("error: uart not connected");
    return;
  }
  if (this->state_ != IrtransState::IDLE || !this->device_ready_) {
    this->set_status_("error: busy");
    return;
  }

  DecodedPayload decoded = decode_payload(payload);
  if (!decoded.ok) {
    this->set_status_(std::string("error: ") + decoded.error);
    ESP_LOGW(TAG, "Transmit payload rejected: %s", decoded.error.c_str());
    return;
  }

  memset(&this->work_, 0, sizeof(this->work_));
  memcpy(&this->work_, decoded.irdata_bytes.data(), decoded.irdata_bytes.size());
  this->prepare_send_frame_(this->work_);

  this->rx_buf_.clear();
  this->flush_rx_();
  this->state_ = IrtransState::TRANSMIT_SEND;
  this->set_status_("transmitting");
  ESP_LOGI(TAG, "Transmit started (freq=%u kHz mode=%u ir_length=%u frame_len=%u)", decoded.freq_khz,
           decoded.mode, decoded.ir_length, this->work_.len);
}

void IRTransUSB::prepare_send_frame_(IRData &ird) {
  ird.command = HOST_SEND;
  if ((ird.address & 0x0f) == 0 && (ird.address & (ADRESS_LOCAL | ADRESS_ALL)) == 0) {
    ird.address = ADRESS_LOCAL;
  }
  if (ird.target_mask == 0) {
    ird.target_mask = 0xffff;
  }
  ird.len = compute_send_len(&ird);
  ird.checksumme = irdata_checksum(&ird);
}

void IRTransUSB::set_status_(const std::string &status) {
  if (this->status_sensor_ != nullptr) {
    this->status_sensor_->publish_state(status);
  }
}

void IRTransUSB::set_connected_(bool connected) {
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(connected);
  }
}

void IRTransUSB::flush_rx_() {
  uint8_t dump;
  while (this->available()) {
    this->read_byte(&dump);
  }
}

bool IRTransUSB::uart_ready_() const {
  if (this->parent_ == nullptr) {
    return false;
  }
  return this->parent_->is_connected();
}

bool IRTransUSB::write_frame_(const IRData &ird, bool with_sequence) {
  if (ird.len == 0 || ird.len > sizeof(IRData)) {
    return false;
  }
  std::vector<uint8_t> frame(reinterpret_cast<const uint8_t *>(&ird),
                             reinterpret_cast<const uint8_t *>(&ird) + ird.len);
  if (with_sequence) {
    frame.push_back(this->io_sequence_++);
  }
  this->write_array(frame);
  return true;
}

bool IRTransUSB::read_into_buffer_(std::vector<uint8_t> &buf, size_t need) {
  while (this->available() > 0 && buf.size() < need) {
    uint8_t b;
    if (!this->read_byte(&b)) {
      break;
    }
    buf.push_back(b);
  }
  return buf.size() >= need;
}

void IRTransUSB::start_handshake_() {
  this->flush_rx_();
  this->rx_buf_.clear();
  this->state_ = IrtransState::HANDSHAKE_SEND;
  this->set_status_("handshake");
}

void IRTransUSB::process_handshake_() {
  if (this->state_ == IrtransState::HANDSHAKE_SEND) {
    if (!this->uart_ready_()) {
      this->next_handshake_ms_ = millis() + 2000;
      this->state_ = IrtransState::IDLE;
      return;
    }

    memset(&this->work_, 0, sizeof(this->work_));
    this->work_.command = HOST_VERSION;
    this->work_.len = 8;
    this->work_.address = VERSION_MAGIC_5;
    this->work_.target_mask = VERSION_MAGIC_2;
    this->work_.ir_length = VERSION_MAGIC_3;
    this->work_.transmit_freq = VERSION_MAGIC_4;
    this->work_.checksumme = irdata_checksum(&this->work_);
    this->flush_rx_();
    this->rx_buf_.clear();
    if (!this->write_frame_(this->work_, this->use_sequence_)) {
      this->fail_("handshake write failed");
      return;
    }
    this->state_ = IrtransState::HANDSHAKE_WAIT_ACK;
    this->state_deadline_ms_ = millis() + 1000;
    return;
  }

  if (this->state_ == IrtransState::HANDSHAKE_WAIT_ACK) {
    if (millis() > this->state_deadline_ms_) {
      this->handshake_attempts_++;
      if (this->handshake_attempts_ >= 5) {
        this->handshake_attempts_ = 0;
        this->next_handshake_ms_ = millis() + 3000;
        this->fail_("handshake timeout");
      } else {
        this->state_ = IrtransState::HANDSHAKE_SEND;
      }
      return;
    }
    if (!this->read_into_buffer_(this->rx_buf_, 1)) {
      return;
    }
    if (this->rx_buf_[0] != 'O') {
      ESP_LOGD(TAG, "Handshake unexpected byte 0x%02X", this->rx_buf_[0]);
      this->rx_buf_.clear();
      this->flush_rx_();
      this->state_ = IrtransState::HANDSHAKE_SEND;
      return;
    }
    this->rx_buf_.clear();
    this->state_ = IrtransState::HANDSHAKE_WAIT_VERSION;
    this->state_deadline_ms_ = millis() + 500;
    return;
  }

  if (this->state_ == IrtransState::HANDSHAKE_WAIT_VERSION) {
    if (millis() > this->state_deadline_ms_ && this->rx_buf_.size() < 8) {
      this->handshake_attempts_++;
      this->state_ = IrtransState::HANDSHAKE_SEND;
      return;
    }
    if (!this->read_into_buffer_(this->rx_buf_, 8)) {
      return;
    }

    this->version_.assign(reinterpret_cast<const char *>(this->rx_buf_.data()), 8);
    while (!this->version_.empty() && (this->version_.back() == '\0' || this->version_.back() == ' ')) {
      this->version_.pop_back();
    }

    // Optional capability bytes — drain briefly without failing.
    this->rx_buf_.clear();
    this->state_deadline_ms_ = millis() + 50;

    this->device_ready_ = true;
    this->handshake_attempts_ = 0;
    this->set_connected_(true);
    this->set_status_("idle");
    this->state_ = IrtransState::IDLE;
    ESP_LOGI(TAG, "IRTrans connected, version='%s'", this->version_.c_str());
  }
}

void IRTransUSB::process_learn_() {
  if (this->state_ == IrtransState::LEARN_SEND_CMD) {
    if (!this->write_frame_(this->work_, this->use_sequence_)) {
      this->fail_("learn write failed");
      return;
    }
    this->rx_buf_.clear();
    this->state_ = IrtransState::LEARN_WAIT_ACK;
    this->state_deadline_ms_ = millis() + 500;
    return;
  }

  if (this->state_ == IrtransState::LEARN_WAIT_ACK) {
    if (millis() > this->state_deadline_ms_) {
      this->fail_("learn command not acknowledged");
      return;
    }
    if (!this->read_into_buffer_(this->rx_buf_, 1)) {
      return;
    }
    if (this->rx_buf_[0] != 'O') {
      this->fail_("learn unexpected ack");
      return;
    }
    this->rx_buf_.clear();
    this->state_ = IrtransState::LEARN_WAIT_LEN;
    this->state_deadline_ms_ = millis() + this->learn_timeout_ms_;
    return;
  }

  if (this->state_ == IrtransState::LEARN_WAIT_LEN) {
    if (millis() > this->state_deadline_ms_) {
      uint8_t cancel = 1;
      this->write_array(&cancel, 1);
      this->fail_("learn timeout");
      return;
    }
    if (!this->read_into_buffer_(this->rx_buf_, 1)) {
      return;
    }
    this->learn_len_ = this->rx_buf_[0];
    if (this->learn_len_ <= IRTRANS_HEADER_LEN) {
      // Too short: keep waiting for a real learned frame length.
      this->rx_buf_.clear();
      return;
    }
    if (this->learn_len_ > sizeof(IRData)) {
      this->fail_("learn frame too large");
      return;
    }
    this->state_ = IrtransState::LEARN_WAIT_BODY;
    this->state_deadline_ms_ = millis() + 2000;
    return;
  }

  if (this->state_ == IrtransState::LEARN_WAIT_BODY) {
    if (millis() > this->state_deadline_ms_) {
      this->fail_("learn body timeout");
      return;
    }
    if (!this->read_into_buffer_(this->rx_buf_, this->learn_len_)) {
      return;
    }

    memset(&this->work_, 0, sizeof(this->work_));
    memcpy(&this->work_, this->rx_buf_.data(), this->rx_buf_.size());

    ESP_LOGI(TAG, "Learn frame len=%u status=0x%02X mode=%u freq_raw=%u ir_length=%u", this->learn_len_,
             this->work_.command, this->work_.mode, this->work_.transmit_freq, this->work_.ir_length);

    this->work_.command = 0;
    if (this->work_.ir_length < IRTRANS_CODE_LEN) {
      this->work_.data[this->work_.ir_length] = 0;
    }
    this->work_.len = this->learn_len_;
    this->finish_learn_ok_(this->work_);
  }
}

void IRTransUSB::finish_learn_ok_(IRData &ird) {
  const std::string payload = encode_payload(ird);
  if (this->last_payload_sensor_ != nullptr) {
    this->last_payload_sensor_->publish_state(payload);
  }
  this->state_ = IrtransState::IDLE;
  this->set_status_("ok");
  ESP_LOGI(TAG, "Learn OK freq_khz=%u mode=%u", decode_freq_khz(ird.transmit_freq), ird.mode);
}

void IRTransUSB::process_transmit_() {
  if (this->state_ == IrtransState::TRANSMIT_SEND) {
    if (!this->write_frame_(this->work_, this->use_sequence_)) {
      this->fail_("transmit write failed");
      return;
    }
    this->rx_buf_.clear();
    this->state_ = IrtransState::TRANSMIT_WAIT_ACK;
    this->state_deadline_ms_ = millis() + 1000;
    return;
  }

  if (this->state_ == IrtransState::TRANSMIT_WAIT_ACK) {
    if (millis() > this->state_deadline_ms_) {
      this->fail_("transmit ack timeout");
      return;
    }
    if (!this->read_into_buffer_(this->rx_buf_, 1)) {
      return;
    }
    if (this->rx_buf_[0] == 'O') {
      this->state_ = IrtransState::IDLE;
      this->set_status_("ok");
      ESP_LOGI(TAG, "Transmit OK");
      return;
    }
    if (this->rx_buf_[0] == 'R') {
      this->rx_buf_.clear();
      this->state_ = IrtransState::TRANSMIT_SEND;
      ESP_LOGW(TAG, "Transmit resend requested");
      return;
    }
    this->fail_("transmit unexpected ack");
  }
}

void IRTransUSB::fail_(const std::string &message) {
  ESP_LOGW(TAG, "%s", message.c_str());
  this->flush_rx_();
  this->rx_buf_.clear();
  this->state_ = IrtransState::IDLE;
  this->set_status_(std::string("error: ") + message);
}

}  // namespace irtrans_usb
}  // namespace esphome
