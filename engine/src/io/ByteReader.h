#pragma once
// Begrenzter Little-Endian-Leser. Fehler (Lesen ueber das Ende) setzen ok() auf false, liefern Nullwerte und crashen nie.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace fleet {

class ByteReader {
 public:
  ByteReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

  bool ok() const { return ok_; }
  size_t pos() const { return pos_; }
  size_t size() const { return size_; }
  size_t remaining() const { return size_ - pos_; }
  bool at_end() const { return pos_ == size_; }

  uint8_t U8() { return Take(1) ? data_[pos_ - 1] : 0; }
  uint16_t U16() {
    if (!Take(2)) return 0;
    return static_cast<uint16_t>(data_[pos_ - 2] | (data_[pos_ - 1] << 8));
  }
  uint32_t U32() {
    if (!Take(4)) return 0;
    const uint8_t* p = data_ + pos_ - 4;
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
  }
  float F32() {
    const uint32_t u = U32();
    float f;
    std::memcpy(&f, &u, sizeof f);
    return f;
  }
  // n Bytes lesen; nullptr bei Fehler.
  const uint8_t* Bytes(size_t n) { return Take(n) ? data_ + pos_ - n : nullptr; }
  void Skip(size_t n) { Take(n); }
  void Seek(size_t p) {
    if (p > size_) {
      ok_ = false;
    } else {
      pos_ = p;
    }
  }
  // u16 Laenge + ASCII. Nur druckbare Zeichen (32..126), sonst Fehler.
  std::string String() {
    const uint16_t n = U16();
    const uint8_t* b = Bytes(n);
    if (!b) return std::string();
    for (uint16_t i = 0; i < n; ++i) {
      if (b[i] < 32 || b[i] > 126) {
        ok_ = false;
        return std::string();
      }
    }
    return std::string(reinterpret_cast<const char*>(b), n);
  }

 private:
  bool Take(size_t n) {
    if (!ok_ || n > size_ - pos_) {
      ok_ = false;
      return false;
    }
    pos_ += n;
    return true;
  }

  const uint8_t* data_;
  size_t size_;
  size_t pos_ = 0;
  bool ok_ = true;
};

}  // namespace fleet
