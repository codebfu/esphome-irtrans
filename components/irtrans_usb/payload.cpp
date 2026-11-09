#include "payload.h"

#include "esphome/components/json/json_util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace irtrans_usb {

static size_t stored_irdata_size(const IRData &ird) {
  size_t need = IRTRANS_HEADER_LEN + ird.ir_length + 1;  // +1 for trailing NUL used by irserver
  if (need < IRTRANS_HEADER_LEN + 1) {
    need = IRTRANS_HEADER_LEN + 1;
  }
  if (need > sizeof(IRData)) {
    need = sizeof(IRData);
  }
  // Prefer wire length when present and plausible
  if (ird.len > IRTRANS_HEADER_LEN && ird.len <= sizeof(IRData)) {
    need = ird.len;
  }
  return need;
}

std::string encode_payload(const IRData &ird) {
  const size_t n = stored_irdata_size(ird);
  std::vector<uint8_t> bytes(reinterpret_cast<const uint8_t *>(&ird),
                             reinterpret_cast<const uint8_t *>(&ird) + n);
  const std::string b64 = base64_encode(bytes.data(), bytes.size());
  const uint8_t freq = decode_freq_khz(ird.transmit_freq);

  // Keep data_b64 as the opaque source of truth; expose derived fields for wiki annotation.
  std::string out = "{\"v\":1,\"fmt\":\"irtrans-irdata\",\"data_b64\":\"";
  out += b64;
  out += "\",\"freq_khz\":";
  out += to_string(freq);
  out += ",\"mode\":";
  out += to_string(ird.mode);
  out += ",\"ir_length\":";
  out += to_string(ird.ir_length);
  out += "}";
  return out;
}

DecodedPayload decode_payload(const std::string &json) {
  DecodedPayload result;
  bool parsed = json::parse_json(json, [&result](JsonObject root) -> bool {
    if (!root.containsKey("data_b64")) {
      result.error = "missing data_b64";
      return false;
    }
    const std::string b64 = root["data_b64"].as<std::string>();
    if (b64.empty()) {
      result.error = "empty data_b64";
      return false;
    }
    result.irdata_bytes = base64_decode(b64);
    if (result.irdata_bytes.size() < IRTRANS_HEADER_LEN) {
      result.error = "IRDATA too short";
      return false;
    }
    if (result.irdata_bytes.size() > sizeof(IRData)) {
      result.error = "IRDATA too long";
      return false;
    }
    const auto *ird = reinterpret_cast<const IRData *>(result.irdata_bytes.data());
    result.freq_khz = decode_freq_khz(ird->transmit_freq);
    result.mode = ird->mode;
    result.ir_length = ird->ir_length;
    result.ok = true;
    return true;
  });

  if (!parsed && result.error.empty()) {
    result.error = "invalid JSON";
  }
  return result;
}

}  // namespace irtrans_usb
}  // namespace esphome
