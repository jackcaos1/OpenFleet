#pragma once
// Software-Mischer (plattformfrei): mischt beliebig viele Stimmen aus PCM-Clips zu 16-Bit-Stereo.
// Die Plattform (SDL auf dem Desktop, AAudio auf Android) ruft Mix() aus ihrem Audio-Thread auf; alle anderen
// Methoden duerfen aus dem Spiel-Thread kommen (intern durch einen Mutex geschuetzt).
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "formats/Wav.h"

namespace fleet {

class Mixer {
 public:
  using VoiceId = int;
  static const VoiceId kNone = 0;

  explicit Mixer(int output_rate = 44100) : rate_(output_rate) {}
  int output_rate() const { return rate_; }

  // gain 0..1, pan -1 (links) .. +1 (rechts). Gibt kNone zurueck, wenn der Clip leer ist.
  VoiceId Play(std::shared_ptr<const PcmClip> clip, float gain, float pan = 0.0f, bool loop = false);
  void Set(VoiceId id, float gain, float pan);
  void Stop(VoiceId id);
  void StopAll();
  bool IsPlaying(VoiceId id);
  void SetMasterGain(float g);
  int active_voices();

  // Schreibt frames Stereo-Frames (L,R verschachtelt).
  void Mix(int16_t* out, int frames);

 private:
  struct Voice {
    VoiceId id = 0;
    std::shared_ptr<const PcmClip> clip;
    double pos = 0;  // Position im Clip in Frames (Bruchteil fuer Resampling)
    double step = 1;  // Frames des Clips je Ausgabeframe
    float gain = 1, pan = 0;
    bool loop = false;
  };
  int rate_;
  std::mutex mu_;
  std::vector<Voice> voices_;
  VoiceId next_id_ = 1;
  float master_ = 1.0f;
};

}  // namespace fleet
