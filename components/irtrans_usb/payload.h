#pragma once

#include <string>
#include <vector>

#include "irdata.h"

namespace esphome {
namespace irtrans_usb {

struct DecodedPayload {
  bool ok{false};
  std::string error;
  std::vector<uint8_t> irdata_bytes;
  uint8_t freq_khz{0};
  uint8_t mode{0};
  uint8_t ir_length{0};
};

std::string encode_payload(const IRData &ird);
DecodedPayload decode_payload(const std::string &json);

}  // namespace irtrans_usb
}  // namespace esphome
