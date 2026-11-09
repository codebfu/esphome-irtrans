#pragma once

#include <cstdint>
#include <cstring>

namespace esphome {
namespace irtrans_usb {

// IRTrans IRDATA layout (packed), based on asterics/IRTrans-Server remote.h
static const uint8_t IRTRANS_TIME_LEN = 8;
static const uint16_t IRTRANS_CODE_LEN = 211;
static const uint16_t IRTRANS_CODE_LEN_18 = (IRTRANS_CODE_LEN - 40);
static const uint16_t IRTRANS_CODE_LEN_SINGLE = (IRTRANS_CODE_LEN - 10);
static const uint16_t IRTRANS_RAW_EXTRA = IRTRANS_TIME_LEN * 4 + 2;
static const uint16_t IRTRANS_HEADER_LEN = 44;  // bytes before data[]

static const uint8_t HOST_LEARNIR = 18;
static const uint8_t HOST_VERSION = 2;
static const uint8_t HOST_SEND = 6;

static const uint8_t ADRESS_LOCAL = 16;
static const uint8_t ADRESS_ALL = 32;

static const uint8_t VERSION_MAGIC_1 = 0x39;
static const uint16_t VERSION_MAGIC_2 = 0x640b;
static const uint8_t VERSION_MAGIC_3 = 0xb3;
static const uint8_t VERSION_MAGIC_4 = 0x8b;
static const uint8_t VERSION_MAGIC_5 = 0x27;

static const uint8_t SPECIAL_IR_MODE = 159;
static const uint8_t RAW_DATA = 16;
static const uint8_t TIMECOUNT_18 = 128;
static const uint8_t PULSE_200 = 131;
static const uint8_t RC6_DATA = 8;

#pragma pack(push, 1)
struct IRData {
  uint8_t len;             // 0
  uint8_t checksumme;      // 1
  uint8_t command;         // 2
  uint8_t address;         // 3
  uint16_t target_mask;    // 4
  uint8_t ir_length;       // 6
  uint8_t transmit_freq;   // 7  carrier / modulation frequency encoding
  uint8_t mode;            // 8  modulation / encoding mode
  int16_t pause_len[IRTRANS_TIME_LEN];  // 9
  int16_t pulse_len[IRTRANS_TIME_LEN];  // 25
  uint8_t time_cnt;        // 41
  uint8_t ir_repeat;       // 42
  uint8_t repeat_pause;    // 43
  uint8_t data[IRTRANS_CODE_LEN];  // 44
};
#pragma pack(pop)

static_assert(sizeof(IRData) == IRTRANS_HEADER_LEN + IRTRANS_CODE_LEN, "IRData size mismatch");

inline uint8_t irdata_checksum(const IRData *ir) {
  uint8_t cs = 0;
  const auto *bytes = reinterpret_cast<const uint8_t *>(ir);
  for (uint8_t i = 2; i < ir->len; i++) {
    cs = static_cast<uint8_t>(cs + bytes[i]);
  }
  return cs;
}

inline uint8_t compute_send_len(const IRData *ir) {
  if (ir->mode == TIMECOUNT_18) {
    return static_cast<uint8_t>((sizeof(IRData) - IRTRANS_CODE_LEN_18) + ir->ir_length);
  }
  if ((ir->mode & SPECIAL_IR_MODE) == PULSE_200) {
    return static_cast<uint8_t>((sizeof(IRData) - IRTRANS_CODE_LEN_SINGLE) + ir->ir_length);
  }
  if (ir->mode & RAW_DATA) {
    return static_cast<uint8_t>((sizeof(IRData) - (IRTRANS_CODE_LEN + IRTRANS_RAW_EXTRA)) + ir->ir_length);
  }
  uint8_t len = static_cast<uint8_t>((sizeof(IRData) - IRTRANS_CODE_LEN) + ir->ir_length);
  if (ir->mode & RC6_DATA) {
    len++;
  }
  return len;
}

inline uint8_t decode_freq_khz(uint8_t transmit_freq) {
  if (transmit_freq == 255) {
    return 455;
  }
  if (transmit_freq & 128) {
    return static_cast<uint8_t>((transmit_freq & 127) * 4);
  }
  return transmit_freq;
}

}  // namespace irtrans_usb
}  // namespace esphome
