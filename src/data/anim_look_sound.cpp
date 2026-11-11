// A look's sounds with effects: see anim_look_sound.h.

#include "data/anim_look_sound.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

namespace data {

namespace {

bool Moved(float value, float fallback) { return value < fallback - 0.0005f || value > fallback + 0.0005f; }

SoundCue Cue(float in, uint32_t at, uint32_t voice_of, const LookSound& sound) {
  SoundCue c;
  c.in = std::max(0.0f, in);
  c.at = at;
  c.voice_of = voice_of;
  c.sound = sound;
  return c;
}

uint32_t U32(const std::string& b, size_t at) {
  uint32_t v;
  memcpy(&v, b.data() + at, 4);
  return v;
}
uint16_t U16(const std::string& b, size_t at) {
  uint16_t v;
  memcpy(&v, b.data() + at, 2);
  return v;
}

// IMA ADPCM, as #154's proof (and the bench's server) decode it: blocks of `align` bytes - each channel's 4-byte header
// (its first sample, its step index) - then 4-bit codes, 8 a channel in turn.
constexpr int kImaStep[89] = {7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,
                              25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,
                              88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,   253,   279,
                              307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,   876,   963,
                              1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,  3327,
                              3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487,
                              12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
constexpr int kImaIndex[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

bool DecodeIma(const std::string& data, int channels, int align, std::string* pcm16) {
  if (channels < 1 || channels > 2 || align < 4 * channels) return false;
  std::vector<int16_t> out;
  for (size_t block = 0; block + align <= data.size(); block += align) {
    int pred[2] = {0, 0}, idx[2] = {0, 0};
    std::vector<int16_t> chans[2];
    for (int c = 0; c < channels; ++c) {
      int16_t first;
      memcpy(&first, data.data() + block + 4 * c, 2);
      pred[c] = first;
      idx[c] = std::min(88, static_cast<int>(static_cast<uint8_t>(data[block + 4 * c + 2])));
      chans[c].push_back(first);
    }
    const auto code = [&](int c, int n) {
      const int step = kImaStep[idx[c]];
      int diff = step >> 3;
      if (n & 1) diff += step >> 2;
      if (n & 2) diff += step >> 1;
      if (n & 4) diff += step;
      pred[c] = std::max(-32768, std::min(32767, (n & 8) ? pred[c] - diff : pred[c] + diff));
      idx[c] = std::max(0, std::min(88, idx[c] + kImaIndex[n]));
      chans[c].push_back(static_cast<int16_t>(pred[c]));
    };
    for (size_t p = block + 4 * channels; p + 4 * channels <= block + align; p += 4 * channels) {
      for (int c = 0; c < channels; ++c) {
        for (int k = 0; k < 4; ++k) {
          const uint8_t byte = static_cast<uint8_t>(data[p + 4 * c + k]);
          code(c, byte & 15);
          code(c, byte >> 4);
        }
      }
    }
    for (size_t i = 0; i < chans[0].size(); ++i) {
      for (int c = 0; c < channels; ++c) out.push_back(chans[c][i]);
    }
  }
  pcm16->assign(reinterpret_cast<const char*>(out.data()), out.size() * 2);
  return !out.empty();
}

}  // namespace

bool SoundHasEffects(const LookSound& s) {
  return Moved(s.pitch, 1.0f) || Moved(s.shift, 0.0f) || Moved(s.muffle, 0.0f) || Moved(s.echo, 0.0f) ||
         Moved(s.reverb, 0.0f) || Moved(s.volume, 1.0f);
}

std::vector<SoundCue> LookCastCues(const LookParts& parts, uint32_t caster) {
  std::vector<SoundCue> out;
  for (const LookSound& s : parts.sounds) {
    if (s.anchor == LookAnchor::kCast && SoundHasEffects(s)) out.push_back(Cue(s.at, caster, caster, s));
  }
  return out;
}

std::vector<SoundCue> LookHitCues(const LookParts& parts, uint32_t caster, uint32_t target, const LookTiming& timing) {
  std::vector<SoundCue> out;
  for (const LookSound& s : parts.sounds) {
    if (!SoundHasEffects(s)) continue;
    if (s.anchor == LookAnchor::kRelease) out.push_back(Cue(timing.release_in + s.at, caster, caster, s));
    if (s.anchor == LookAnchor::kHit) out.push_back(Cue(timing.hit_in + s.at, target, caster, s));
  }
  return out;
}

std::vector<SoundCue> LookMissCues(const LookParts& parts, uint32_t caster, const LookTiming& timing) {
  std::vector<SoundCue> out;
  for (const LookSound& s : parts.sounds) {
    if (s.anchor == LookAnchor::kRelease && SoundHasEffects(s)) out.push_back(Cue(timing.release_in + s.at, caster, caster, s));
  }
  return out;
}

std::vector<SoundCue> LookPointCues(const LookParts& parts, uint32_t caster, FlightPoint point, const LookTiming& timing) {
  std::vector<SoundCue> out = LookMissCues(parts, caster, timing);
  for (const LookSound& s : parts.sounds) {
    if (s.anchor != LookAnchor::kHit || !SoundHasEffects(s)) continue;
    SoundCue c = Cue(timing.hit_in + s.at, caster, caster, s);
    c.at_point = true;
    c.point = point;
    out.push_back(c);
  }
  return out;
}

std::vector<SoundCue> LookLeapCues(const LookParts& parts, uint32_t from, uint32_t to, float hit_in) {
  std::vector<SoundCue> out;
  for (const LookSound& s : parts.sounds) {
    if (s.anchor == LookAnchor::kHit && SoundHasEffects(s)) out.push_back(Cue(hit_in + s.at, to, from, s));
  }
  return out;
}

SoundMix MixFor(const LookSound& s) {
  SoundMix m;
  m.pitch = s.pitch;
  m.gain = s.volume;
  m.shift = static_cast<int>(s.shift < 0 ? s.shift - 0.5f : s.shift + 0.5f);
  m.dry = m.shift != 0 ? 0.0f : 1.0f;
  if (s.muffle > 0.0f) m.muffle_hf = std::max(0.01f, (1.0f - s.muffle) * (1.0f - s.muffle));
  if (s.echo > 0.0f) {
    m.echo_feedback = s.echo;
    m.echo_gain = 0.8f;
  }
  m.reverb_gain = std::max(0.0f, s.reverb);
  return m;
}

bool DecodeWav(const std::string& b, Pcm* pcm, std::string* why) {
  if (b.size() < 12 || b.compare(0, 4, "RIFF") != 0 || b.compare(8, 4, "WAVE") != 0) {
    *why = "not a RIFF WAVE file (a BMU - the voices' MP3 - or something else)";
    return false;
  }
  int kind = 0, align = 0;
  Pcm out;
  std::string data;
  for (size_t at = 12; at + 8 <= b.size();) {
    const std::string id = b.substr(at, 4);
    const uint32_t size = U32(b, at + 4);
    const size_t body = at + 8;
    if (body + size > b.size() && id != "data") break;
    if (id == "fmt " && size >= 16) {
      kind = U16(b, body);
      out.channels = U16(b, body + 2);
      out.rate = static_cast<int>(U32(b, body + 4));
      align = U16(b, body + 12);
      out.bits = U16(b, body + 14);
    } else if (id == "data") {
      data = b.substr(body, std::min<size_t>(size, b.size() - body));
    }
    at = body + size + (size & 1);
  }
  if (kind == 17) {
    if (!DecodeIma(data, out.channels, align, &out.data)) {
      *why = "an IMA ADPCM sound that didn't decode";
      return false;
    }
    out.bits = 16;
  } else if (kind == 1) {
    out.data = data;
  } else {
    *why = "not PCM or IMA ADPCM (format " + std::to_string(kind) + (kind == 85 ? ": MP3" : "") + ")";
    return false;
  }
  if (out.channels < 1 || out.channels > 2 || (out.bits != 8 && out.bits != 16) || out.rate <= 0 || out.data.empty()) {
    *why = "an unsupported layout (" + std::to_string(out.channels) + " channels, " + std::to_string(out.bits) + " bits)";
    return false;
  }
  *pcm = out;
  return true;
}

}  // namespace data
