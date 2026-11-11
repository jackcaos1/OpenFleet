#include "formats/Wav.h"

#include <cstring>

namespace fleet {
namespace {
uint32_t U32(const uint8_t* p) { return p[0] | (p[1] << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint16_t U16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
bool Fail(std::string* err, const char* m) {
  if (err) *err = m;
  return false;
}
}  // namespace

bool ParseWav(const uint8_t* d, size_t n, PcmClip* out, std::string* err) {
  if (n < 12 || std::memcmp(d, "RIFF", 4) != 0 || std::memcmp(d + 8, "WAVE", 4) != 0) return Fail(err, "kein WAV");
  int tag = 0, ch = 0, bits = 0;
  uint32_t rate = 0;
  const uint8_t* pcm = nullptr;
  size_t pcm_size = 0;
  for (size_t i = 12; i + 8 <= n;) {
    const uint32_t sz = U32(d + i + 4);
    const size_t body = i + 8;
    const size_t avail = n - body;
    const size_t len = sz > avail ? avail : sz;  // abgeschnittene Datei: nehmen, was da ist
    if (std::memcmp(d + i, "fmt ", 4) == 0 && len >= 16) {
      tag = U16(d + body);
      ch = U16(d + body + 2);
      rate = U32(d + body + 4);
      bits = U16(d + body + 14);
    } else if (std::memcmp(d + i, "data", 4) == 0) {
      pcm = d + body;
      pcm_size = len;
      break;
    }
    i = body + len + (len & 1);
  }
  if (!pcm) return Fail(err, "kein data-Block");
  if (tag != 1) return Fail(err, "nicht unkomprimiert (PCM)");
  if ((bits != 8 && bits != 16) || ch < 1 || ch > 2 || rate == 0) return Fail(err, "Format nicht unterstuetzt");
  PcmClip c;
  c.sample_rate = static_cast<int>(rate);
  c.channels = ch;
  const size_t bytes_per = static_cast<size_t>(bits / 8);
  const size_t count = pcm_size / bytes_per / static_cast<size_t>(ch) * static_cast<size_t>(ch);
  c.samples.resize(count);
  for (size_t i = 0; i < count; ++i) {
    if (bits == 8) c.samples[i] = static_cast<int16_t>((static_cast<int>(pcm[i]) - 128) * 256);
    else c.samples[i] = static_cast<int16_t>(U16(pcm + i * 2));
  }
  *out = std::move(c);
  return true;
}

}  // namespace fleet
