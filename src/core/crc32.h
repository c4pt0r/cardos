#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// Standard CRC-32 (IEEE 802.3, poly 0xEDB88320). Pure header — used by the
// serial upload protocol to verify transferred app files; native-tested.
namespace crc32 {

inline uint32_t update(uint32_t crc, const uint8_t* data, size_t n) {
  crc = ~crc;
  for (size_t i = 0; i < n; i++) {
    crc ^= data[i];
    for (int k = 0; k < 8; k++)
      crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
  }
  return ~crc;
}

inline uint32_t compute(const uint8_t* data, size_t n) {
  return update(0, data, n);
}
inline uint32_t of(const std::string& s) {
  return compute(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

}  // namespace crc32
