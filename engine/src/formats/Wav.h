#pragma once
// RIFF/WAVE, unkomprimiert (PCM, 8 oder 16 Bit, mono oder stereo). Alle Originaldateien (sounds/, wav/) haben dieses Format.
#include <cstdint>
#include <string>
#include <vector>

namespace fleet {

struct PcmClip {
  int sample_rate = 22050;
  int channels = 1;
  std::vector<int16_t> samples;  // verschachtelt: frame*channels + kanal
  size_t frames() const { return channels > 0 ? samples.size() / static_cast<size_t>(channels) : 0; }
};

bool ParseWav(const uint8_t* data, size_t size, PcmClip* out, std::string* err = nullptr);

}  // namespace fleet
