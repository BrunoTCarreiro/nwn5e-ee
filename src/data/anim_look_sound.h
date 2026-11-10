#pragma once

// A look's sounds with effects (owner, 2026-11-10: "build the in-game sound player"; #154's proof, SF-1..SF-6 passed):
// the game's PlaySound can't pitch, muffle, echo or reverb, so a sound with any effect leaves the look's script
// (anim_look.cpp's LookCastScript / LookHitScript / LookLeapScript) and src/core/anim_sound_player.cpp plays it on its
// own OpenAL source with EFX, at the object it belongs to. Here: which sounds, when and where, the mix (as the bench's
// preview, spell_bench/public/sound.js) and the game's WAVs read as PCM.

#include <stdint.h>

#include <string>
#include <vector>

#include "data/anim_look.h"

namespace data {

// Any effect not at its default (a voice alone isn't one: the script's GetGender plays it).
bool SoundHasEffects(const LookSound& sound);

// One sound for the player: `in` seconds from now, at object `at`; a voice line plays only when `voice_of`'s gender is
// its voice (as the script's OBJECT_SELF: the caster, or the leap's `from`).
struct SoundCue {
  float in = 0.0f;
  uint32_t at = 0, voice_of = 0;
  LookSound sound;
};
// The sounds with effects of LookCastScript (at the caster), LookHitScript (release ones at the caster, hit ones at the
// target) and LookLeapScript (hit ones at `to`), with the same times.
std::vector<SoundCue> LookCastCues(const LookParts& parts, uint32_t caster);
std::vector<SoundCue> LookHitCues(const LookParts& parts, uint32_t caster, uint32_t target, const LookTiming& timing);
std::vector<SoundCue> LookLeapCues(const LookParts& parts, uint32_t from, uint32_t to, float hit_in);
// A miss's (LookMissScript): the release ones only - it fizzles, no hit sounds.
std::vector<SoundCue> LookMissCues(const LookParts& parts, uint32_t caster, const LookTiming& timing);

// A sound's effects as OpenAL / EFX settings:
//   pitch      AL_PITCH (faster and higher);  gain  AL_GAIN (the volume; the source's AL_MAX_GAIN raised to it);
//   dry        the direct path's gain: 0 when the pitch shifter replaces the sound;
//   muffle_hf  a low-pass's AL_LOWPASS_GAINHF on the direct path and the sends: (1 - muffle)^2, at least 0.01;
//   shift      semitones through the pitch shifter (0: none);
//   echo_*     the echo's feedback and its slot's gain (feedback 0: none), a 0.12 s delay;
//   reverb_gain  the reverb slot's gain (0: none), a 2.5 s decay.
struct SoundMix {
  float pitch = 1.0f, gain = 1.0f, dry = 1.0f, muffle_hf = 1.0f;
  int shift = 0;
  float echo_feedback = 0.0f, echo_gain = 0.0f, reverb_gain = 0.0f;
};
SoundMix MixFor(const LookSound& sound);

// A game WAV as PCM: plain PCM as it is, IMA ADPCM (format 17, the game's effect sounds) decoded to 16-bit. False and
// *why for anything else (MP3 inside a WAV - format 85 - or a BMU, the voices).
struct Pcm {
  int channels = 0, bits = 0, rate = 0;
  std::string data;
};
bool DecodeWav(const std::string& bytes, Pcm* pcm, std::string* why);

}  // namespace data
