#include "audio/Mixer.h"

#include <algorithm>
#include <cmath>

namespace fleet {

Mixer::VoiceId Mixer::Play(std::shared_ptr<const PcmClip> clip, float gain, float pan, bool loop) {
  if (!clip || clip->frames() == 0) return kNone;
  Voice v;
  v.clip = std::move(clip);
  v.step = static_cast<double>(v.clip->sample_rate) / static_cast<double>(rate_);
  v.gain = gain;
  v.pan = pan;
  v.loop = loop;
  std::lock_guard<std::mutex> lock(mu_);
  v.id = next_id_++;
  voices_.push_back(std::move(v));
  return voices_.back().id;
}

void Mixer::Set(VoiceId id, float gain, float pan) {
  std::lock_guard<std::mutex> lock(mu_);
  for (Voice& v : voices_)
    if (v.id == id) {
      v.gain = gain;
      v.pan = pan;
    }
}

void Mixer::Stop(VoiceId id) {
  std::lock_guard<std::mutex> lock(mu_);
  voices_.erase(std::remove_if(voices_.begin(), voices_.end(), [id](const Voice& v) { return v.id == id; }), voices_.end());
}

void Mixer::StopAll() {
  std::lock_guard<std::mutex> lock(mu_);
  voices_.clear();
}

bool Mixer::IsPlaying(VoiceId id) {
  std::lock_guard<std::mutex> lock(mu_);
  for (const Voice& v : voices_)
    if (v.id == id) return true;
  return false;
}

void Mixer::SetMasterGain(float g) {
  std::lock_guard<std::mutex> lock(mu_);
  master_ = g;
}

int Mixer::active_voices() {
  std::lock_guard<std::mutex> lock(mu_);
  return static_cast<int>(voices_.size());
}

void Mixer::Mix(int16_t* out, int frames) {
  std::vector<float> acc(static_cast<size_t>(frames) * 2, 0.0f);
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (Voice& v : voices_) {
      const PcmClip& c = *v.clip;
      const double total = static_cast<double>(c.frames());
      const int ch = c.channels;
      // gleichbleibende Gesamtlautstaerke ueber den Schwenk (Sinus-/Kosinusgesetz)
      const float a = (std::max(-1.0f, std::min(1.0f, v.pan)) + 1.0f) * 0.78539816f;
      const float gl = v.gain * std::cos(a) * master_ * 1.41421356f, gr = v.gain * std::sin(a) * master_ * 1.41421356f;
      for (int i = 0; i < frames; ++i) {
        if (v.pos >= total) {
          if (!v.loop) break;
          v.pos = std::fmod(v.pos, total);
        }
        const size_t i0 = static_cast<size_t>(v.pos);
        const double f = v.pos - static_cast<double>(i0);
        size_t i1 = i0 + 1;
        if (i1 >= c.frames()) i1 = v.loop ? 0 : i0;
        float l, r;
        if (ch == 1) {
          l = r = (static_cast<float>(c.samples[i0]) * static_cast<float>(1.0 - f) + static_cast<float>(c.samples[i1]) * static_cast<float>(f)) / 32768.0f;
        } else {
          l = (static_cast<float>(c.samples[i0 * 2]) * static_cast<float>(1.0 - f) + static_cast<float>(c.samples[i1 * 2]) * static_cast<float>(f)) / 32768.0f;
          r = (static_cast<float>(c.samples[i0 * 2 + 1]) * static_cast<float>(1.0 - f) + static_cast<float>(c.samples[i1 * 2 + 1]) * static_cast<float>(f)) / 32768.0f;
        }
        acc[static_cast<size_t>(i) * 2] += l * gl;
        acc[static_cast<size_t>(i) * 2 + 1] += r * gr;
        v.pos += v.step;
      }
    }
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(), [](const Voice& v) { return !v.loop && v.pos >= static_cast<double>(v.clip->frames()); }),
                  voices_.end());
  }
  for (size_t i = 0; i < acc.size(); ++i) {
    float x = acc[i];
    x = x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);  // harte Begrenzung
    out[i] = static_cast<int16_t>(x * 32767.0f);
  }
}

}  // namespace fleet
