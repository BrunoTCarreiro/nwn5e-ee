// The look's sounds with effects (owner, 2026-11-10: "build the in-game sound player"): which sounds the DLL's player
// takes from the scripts, when and where each plays, how its effects map onto OpenAL EFX (as the bench's preview), and
// the game's WAVs read as PCM.

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#include "check.h"
#include "data/anim_look.h"
#include "data/anim_look_sound.h"

namespace {

data::LookSound Sound(data::LookAnchor anchor, float at, const char* resref) {
  data::LookSound s;
  s.anchor = anchor;
  s.at = at;
  s.resref = resref;
  return s;
}

bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }

std::string Riff(uint16_t format, uint16_t channels, uint32_t rate, uint16_t align, uint16_t bits, const std::string& data) {
  std::string fmt(16, '\0');
  const uint32_t byte_rate = rate * align;
  memcpy(&fmt[0], &format, 2);
  memcpy(&fmt[2], &channels, 2);
  memcpy(&fmt[4], &rate, 4);
  memcpy(&fmt[8], &byte_rate, 4);
  memcpy(&fmt[12], &align, 2);
  memcpy(&fmt[14], &bits, 2);
  const auto chunk = [](const char* id, const std::string& body) {
    std::string out(id, 4);
    const uint32_t size = static_cast<uint32_t>(body.size());
    out.append(reinterpret_cast<const char*>(&size), 4);
    return out + body;
  };
  const std::string body = std::string("WAVE") + chunk("fmt ", fmt) + chunk("data", data);
  const uint32_t size = static_cast<uint32_t>(body.size());
  return std::string("RIFF") + std::string(reinterpret_cast<const char*>(&size), 4) + body;
}

}  // namespace

TEST(AnimLookSoundHasEffects) {
  data::LookSound s = Sound(data::LookAnchor::kHit, 0, "sim_x");
  CHECK_EQ(data::SoundHasEffects(s), false);
  s.voice = "female";
  CHECK_EQ(data::SoundHasEffects(s), false);  // a voice is the script's (GetGender), not an effect
  for (float data::LookSound::*field : {&data::LookSound::pitch, &data::LookSound::shift, &data::LookSound::muffle,
                                        &data::LookSound::echo, &data::LookSound::reverb, &data::LookSound::volume}) {
    data::LookSound fx = Sound(data::LookAnchor::kHit, 0, "sim_x");
    fx.*field += 0.5f;
    CHECK_EQ(data::SoundHasEffects(fx), true);
  }
}

TEST(AnimLookScriptsLeaveEffectSoundsToThePlayer) {
  data::LookParts p;
  data::LookSound fx_cast = Sound(data::LookAnchor::kCast, 0.2f, "sco_fx");
  fx_cast.pitch = 0.8f;
  data::LookSound fx_release = Sound(data::LookAnchor::kRelease, 0.05f, "spr_fx");
  fx_release.echo = 0.3f;
  data::LookSound fx_hit = Sound(data::LookAnchor::kHit, 0.05f, "sim_fx");
  fx_hit.reverb = 0.4f;
  p.sounds = {Sound(data::LookAnchor::kCast, 0, "sco_plain"), fx_cast, Sound(data::LookAnchor::kRelease, 0, "spr_plain"),
              fx_release, Sound(data::LookAnchor::kHit, 0, "sim_plain"), fx_hit};
  const std::string cast = data::LookCastScript(p);
  CHECK_EQ(cast.find("sco_plain") != std::string::npos && cast.find("sco_fx") == std::string::npos, true);
  const std::string hit = data::LookHitScript(p, 0x11, {0.10f, 1.22f, false});
  CHECK_EQ(hit.find("spr_plain") != std::string::npos && hit.find("sim_plain") != std::string::npos, true);
  CHECK_EQ(hit.find("spr_fx") == std::string::npos && hit.find("sim_fx") == std::string::npos, true);
  const std::string leap = data::LookLeapScript(p, 1.0f, 0x11, 0x12, 1.22f, 1.52f);
  CHECK_EQ(leap.find("sim_plain") != std::string::npos && leap.find("sim_fx") == std::string::npos, true);

  // ... and the player's cues: when (seconds from now), at which object, whose voice.
  const std::vector<data::SoundCue> c = data::LookCastCues(p, 0x10);
  CHECK_EQ(c.size(), 1u);
  CHECK_EQ(c[0].sound.resref == "sco_fx" && Near(c[0].in, 0.2f) && c[0].at == 0x10u && c[0].voice_of == 0x10u, true);
  const std::vector<data::SoundCue> h = data::LookHitCues(p, 0x10, 0x11, {0.10f, 1.22f, false});
  CHECK_EQ(h.size(), 2u);
  CHECK_EQ(h[0].sound.resref == "spr_fx" && Near(h[0].in, 0.15f) && h[0].at == 0x10u && h[0].voice_of == 0x10u, true);
  CHECK_EQ(h[1].sound.resref == "sim_fx" && Near(h[1].in, 1.27f) && h[1].at == 0x11u && h[1].voice_of == 0x10u, true);
  const std::vector<data::SoundCue> l = data::LookLeapCues(p, 0x11, 0x12, 1.52f);
  CHECK_EQ(l.size(), 1u);
  CHECK_EQ(l[0].sound.resref == "sim_fx" && Near(l[0].in, 1.57f) && l[0].at == 0x12u && l[0].voice_of == 0x11u, true);
}

TEST(AnimLookSoundMix) {  // as the bench's preview (spell_bench/public/sound.js) and #154's proof
  data::LookSound s = Sound(data::LookAnchor::kHit, 0, "sim_x");
  const data::SoundMix plain = data::MixFor(s);
  CHECK_EQ(Near(plain.pitch, 1) && Near(plain.gain, 1) && Near(plain.dry, 1) && Near(plain.muffle_hf, 1), true);
  CHECK_EQ(plain.shift == 0 && Near(plain.echo_feedback, 0) && Near(plain.reverb_gain, 0), true);
  s.pitch = 0.8f;
  s.shift = -3;
  s.muffle = 0.5f;
  s.echo = 0.3f;
  s.reverb = 0.4f;
  s.volume = 1.5f;
  const data::SoundMix m = data::MixFor(s);
  CHECK_EQ(Near(m.pitch, 0.8f) && Near(m.gain, 1.5f), true);
  CHECK_EQ(m.shift == -3 && Near(m.dry, 0), true);  // the pitch shifter replaces the sound: its dry path silent
  CHECK_EQ(Near(m.muffle_hf, 0.25f), true);         // (1 - muffle)^2 of the highs
  CHECK_EQ(Near(m.echo_feedback, 0.3f) && Near(m.echo_gain, 0.8f) && Near(m.reverb_gain, 0.4f), true);
  s.muffle = 1;
  CHECK_EQ(Near(data::MixFor(s).muffle_hf, 0.01f), true);  // never quite silent
}

TEST(AnimLookSoundWav) {
  data::Pcm pcm;
  std::string why;
  const std::string samples("\x01\x00\xff\x7f", 4);
  CHECK_EQ(data::DecodeWav(Riff(1, 1, 22050, 2, 16, samples), &pcm, &why), true);
  CHECK_EQ(pcm.channels == 1 && pcm.bits == 16 && pcm.rate == 22050 && pcm.data == samples, true);

  // IMA ADPCM (the game's effect sounds): a block of 36 bytes - the first sample (1000), its step index (0), then 32
  // bytes of 4-bit codes; code 0 adds step >> 3 (0 at the first step), so all 65 samples stay 1000.
  std::string block(36, '\0');
  const int16_t first = 1000;
  memcpy(&block[0], &first, 2);
  CHECK_EQ(data::DecodeWav(Riff(17, 1, 22050, 36, 4, block), &pcm, &why), true);
  CHECK_EQ(pcm.channels == 1 && pcm.bits == 16 && pcm.data.size() == 130u, true);
  int16_t last = 0;
  memcpy(&last, pcm.data.data() + 128, 2);
  CHECK_EQ(last, 1000);

  CHECK_EQ(data::DecodeWav(Riff(85, 1, 22050, 1, 0, std::string(16, '\0')), &pcm, &why), false);  // MP3 in a WAV
  CHECK_EQ(why.find("MP3") != std::string::npos, true);
  CHECK_EQ(data::DecodeWav("BMU V1.0" + std::string(32, '\0'), &pcm, &why), false);
  CHECK_EQ(why.find("RIFF") != std::string::npos, true);
}

TEST(AnimLookMissCues) {  // a miss keeps the release's sounds with effects, not the hit's
  data::LookParts p;
  data::LookSound release = Sound(data::LookAnchor::kRelease, 0.05f, "spr_fx");
  release.echo = 0.3f;
  data::LookSound hit = Sound(data::LookAnchor::kHit, 0.05f, "sim_fx");
  hit.reverb = 0.4f;
  p.sounds = {release, hit};
  const std::vector<data::SoundCue> m = data::LookMissCues(p, 0x10, {0.10f, 1.22f, false});
  CHECK_EQ(m.size(), 1u);
  CHECK_EQ(m[0].sound.resref == "spr_fx" && Near(m[0].in, 0.15f) && m[0].at == 0x10u, true);
}
