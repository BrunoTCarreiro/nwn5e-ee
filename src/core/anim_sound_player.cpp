// The look's sounds with effects, played in game: see anim_sound_player.h. The OpenAL / EFX calls are #154's proof's
// (nwn5e-tools dev/sound_probe.cpp), through the context the game made current; the mix and the WAVs are data::'s
// (src/data/anim_look_sound, tested).

#include "anim_sound_player.h"

#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "creature.h"
#include "creature_update.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "log.h"

namespace {

// OpenAL / EFX constants (al.h, alc.h, efx.h).
constexpr int AL_SOURCE_RELATIVE = 0x202, AL_PITCH = 0x1003, AL_POSITION = 0x1004, AL_BUFFER = 0x1009, AL_GAIN = 0x100A;
constexpr int AL_MAX_GAIN = 0x100E, AL_ORIENTATION = 0x100F, AL_SOURCE_STATE = 0x1010, AL_STOPPED = 0x1014;
constexpr int AL_REFERENCE_DISTANCE = 0x1020, AL_ROLLOFF_FACTOR = 0x1021, AL_MAX_DISTANCE = 0x1023;
constexpr int AL_DISTANCE_MODEL = 0xD000;
constexpr int AL_FORMAT_MONO8 = 0x1100, AL_FORMAT_MONO16 = 0x1101, AL_FORMAT_STEREO8 = 0x1102, AL_FORMAT_STEREO16 = 0x1103;
constexpr int ALC_MAX_AUXILIARY_SENDS = 0x20003;
constexpr int AL_DIRECT_FILTER = 0x20005, AL_AUXILIARY_SEND_FILTER = 0x20006;
constexpr int AL_FILTER_TYPE = 0x8001, AL_FILTER_LOWPASS = 0x0001, AL_LOWPASS_GAIN = 0x0001, AL_LOWPASS_GAINHF = 0x0002;
constexpr int AL_FILTER_NULL = 0;
constexpr int AL_EFFECT_TYPE = 0x8001, AL_EFFECT_REVERB = 0x0001, AL_EFFECT_ECHO = 0x0004, AL_EFFECT_PITCH_SHIFTER = 0x0008;
constexpr int AL_REVERB_DECAY_TIME = 0x0005;
constexpr int AL_ECHO_DELAY = 0x0001, AL_ECHO_LRDELAY = 0x0002, AL_ECHO_DAMPING = 0x0003, AL_ECHO_FEEDBACK = 0x0004;
constexpr int AL_PITCH_SHIFTER_COARSE_TUNE = 0x0001;
constexpr int AL_EFFECTSLOT_EFFECT = 0x0001, AL_EFFECTSLOT_GAIN = 0x0002;
constexpr uint16_t kResourceTypeWav = 4;

constexpr size_t kMostPlaying = 24;     // more at once play plain
constexpr int64_t kTailMs = 3000;       // an echo's or a reverb's tail rings on after its source stops
constexpr float kAboveFeet = 1.0f;      // a creature's sound plays a metre up, at its chest
constexpr float kReferenceMetres = 5.0f, kMaxMetres = 50.0f;  // full volume within 5 m, fading out to 50 m

using GenFn = void(__cdecl*)(int, unsigned*);
using IntFn = void(__cdecl*)(unsigned, int, int);
using FloatFn = void(__cdecl*)(unsigned, int, float);
using Int3Fn = void(__cdecl*)(unsigned, int, int, int, int);
using Float3Fn = void(__cdecl*)(unsigned, int, float, float, float);
using GetIntFn = void(__cdecl*)(unsigned, int, int*);
using SourceFn = void(__cdecl*)(unsigned);
using BufferDataFn = void(__cdecl*)(unsigned, int, const void*, int, int);
using GetErrorFn = int(__cdecl*)();
using GetIntegerFn = int(__cdecl*)(int);
using GetListenerfvFn = void(__cdecl*)(int, float*);
using GetProcFn = void*(__cdecl*)(const char*);
using CurrentContextFn = void*(__cdecl*)();
using ContextsDeviceFn = void*(__cdecl*)(void*);
using AlcIntegervFn = void(__cdecl*)(void*, int, int, int*);
using AlcExtensionFn = char(__cdecl*)(void*, const char*);

struct Al {
  GenFn gen_sources = nullptr, delete_sources = nullptr, gen_buffers = nullptr;
  IntFn sourcei = nullptr;
  FloatFn sourcef = nullptr;
  Float3Fn source3f = nullptr;
  Int3Fn source3i = nullptr;
  GetIntFn get_sourcei = nullptr;
  SourceFn play = nullptr, stop = nullptr;
  BufferDataFn buffer_data = nullptr;
  GetErrorFn get_error = nullptr;
  GetIntegerFn get_integer = nullptr;
  GetListenerfvFn get_listenerfv = nullptr;
  GetProcFn get_proc = nullptr;
  CurrentContextFn current_context = nullptr;
  ContextsDeviceFn contexts_device = nullptr;
  AlcIntegervFn alc_integerv = nullptr;
  AlcExtensionFn alc_extension = nullptr;
  GenFn gen_filters = nullptr, delete_filters = nullptr, gen_effects = nullptr, delete_effects = nullptr;
  GenFn gen_slots = nullptr, delete_slots = nullptr;
  IntFn filteri = nullptr, effecti = nullptr, sloti = nullptr;
  FloatFn filterf = nullptr, effectf = nullptr, slotf = nullptr;
  bool ok = false, efx = false;
  int sends = 0;
};

Al g_al;
bool g_al_tried = false, g_installed = false, g_first_logged = false;
std::string g_al_why;

struct Pending {
  int64_t due = 0;
  data::SoundCue cue;
};
std::vector<Pending> g_pending;

struct Playing {
  unsigned source = 0;
  std::vector<unsigned> filters, effects, slots;
  bool tail = false;
  int64_t stopped_at = 0;  // 0 while it plays
};
std::vector<Playing> g_playing;

std::map<std::string, unsigned> g_buffers;  // resref -> a loaded buffer (kept for the session)
std::set<std::string> g_plain_logged;       // the sounds already logged as played plain

int64_t Now() { return static_cast<int64_t>(GameMs()); }

// OpenAL, once, from the game's own OpenAL32.dll (the context it made current).
const Al& LoadAl() {
  if (g_al_tried) return g_al;
  g_al_tried = true;
  Al a;
  HMODULE dll = GetModuleHandleA("OpenAL32.dll");
  if (!dll) {
    g_al_why = "OpenAL32.dll isn't loaded";
    return g_al;
  }
  const auto get = [&](const char* name) { return reinterpret_cast<void*>(GetProcAddress(dll, name)); };
  a.gen_sources = reinterpret_cast<GenFn>(get("alGenSources"));
  a.delete_sources = reinterpret_cast<GenFn>(get("alDeleteSources"));
  a.gen_buffers = reinterpret_cast<GenFn>(get("alGenBuffers"));
  a.sourcei = reinterpret_cast<IntFn>(get("alSourcei"));
  a.sourcef = reinterpret_cast<FloatFn>(get("alSourcef"));
  a.source3f = reinterpret_cast<Float3Fn>(get("alSource3f"));
  a.source3i = reinterpret_cast<Int3Fn>(get("alSource3i"));
  a.get_sourcei = reinterpret_cast<GetIntFn>(get("alGetSourcei"));
  a.play = reinterpret_cast<SourceFn>(get("alSourcePlay"));
  a.stop = reinterpret_cast<SourceFn>(get("alSourceStop"));
  a.buffer_data = reinterpret_cast<BufferDataFn>(get("alBufferData"));
  a.get_error = reinterpret_cast<GetErrorFn>(get("alGetError"));
  a.get_integer = reinterpret_cast<GetIntegerFn>(get("alGetInteger"));
  a.get_listenerfv = reinterpret_cast<GetListenerfvFn>(get("alGetListenerfv"));
  a.get_proc = reinterpret_cast<GetProcFn>(get("alGetProcAddress"));
  a.current_context = reinterpret_cast<CurrentContextFn>(get("alcGetCurrentContext"));
  a.contexts_device = reinterpret_cast<ContextsDeviceFn>(get("alcGetContextsDevice"));
  a.alc_integerv = reinterpret_cast<AlcIntegervFn>(get("alcGetIntegerv"));
  a.alc_extension = reinterpret_cast<AlcExtensionFn>(get("alcIsExtensionPresent"));
  a.ok = a.gen_sources && a.delete_sources && a.gen_buffers && a.sourcei && a.sourcef && a.source3f && a.source3i &&
         a.get_sourcei && a.play && a.stop && a.buffer_data && a.get_error && a.get_integer && a.get_listenerfv &&
         a.get_proc && a.current_context && a.contexts_device && a.alc_integerv && a.alc_extension;
  if (!a.ok) {
    g_al_why = "an OpenAL call is missing from OpenAL32.dll";
    return g_al;
  }
  const auto efx = [&](const char* name) { return a.get_proc(name); };
  a.gen_filters = reinterpret_cast<GenFn>(efx("alGenFilters"));
  a.delete_filters = reinterpret_cast<GenFn>(efx("alDeleteFilters"));
  a.gen_effects = reinterpret_cast<GenFn>(efx("alGenEffects"));
  a.delete_effects = reinterpret_cast<GenFn>(efx("alDeleteEffects"));
  a.gen_slots = reinterpret_cast<GenFn>(efx("alGenAuxiliaryEffectSlots"));
  a.delete_slots = reinterpret_cast<GenFn>(efx("alDeleteAuxiliaryEffectSlots"));
  a.filteri = reinterpret_cast<IntFn>(efx("alFilteri"));
  a.filterf = reinterpret_cast<FloatFn>(efx("alFilterf"));
  a.effecti = reinterpret_cast<IntFn>(efx("alEffecti"));
  a.effectf = reinterpret_cast<FloatFn>(efx("alEffectf"));
  a.sloti = reinterpret_cast<IntFn>(efx("alAuxiliaryEffectSloti"));
  a.slotf = reinterpret_cast<FloatFn>(efx("alAuxiliaryEffectSlotf"));
  const bool efx_calls = a.gen_filters && a.delete_filters && a.gen_effects && a.delete_effects && a.gen_slots &&
                         a.delete_slots && a.filteri && a.filterf && a.effecti && a.effectf && a.sloti && a.slotf;
  void* context = a.current_context();
  void* device = context ? a.contexts_device(context) : nullptr;
  if (!device) {
    g_al_why = "no OpenAL context is current";
    a.ok = false;
    g_al = a;
    return g_al;
  }
  a.alc_integerv(device, ALC_MAX_AUXILIARY_SENDS, 1, &a.sends);
  a.efx = efx_calls && a.alc_extension(device, "ALC_EXT_EFX") && a.sends > 0;
  g_al = a;
  Log("sound player: OpenAL ready - EFX %s, %d sends a sound", a.efx ? "yes" : "no (pitch and volume only)", a.sends);
  return g_al;
}

// A sound's buffer: loaded once (the game's resource manager, then data::DecodeWav); 0 and *why when it can't be.
unsigned BufferFor(const Al& a, const std::string& resref, std::string* why) {
  const auto found = g_buffers.find(resref);
  if (found != g_buffers.end()) return found->second;
  std::string bytes;
  data::Pcm pcm;
  if (!ReadGameResource(resref, kResourceTypeWav, &bytes, why)) return 0;
  if (!data::DecodeWav(bytes, &pcm, why)) return 0;
  const int format = pcm.channels == 1 ? (pcm.bits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16)
                                       : (pcm.bits == 8 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16);
  unsigned buffer = 0;
  a.get_error();
  a.gen_buffers(1, &buffer);
  a.buffer_data(buffer, format, pcm.data.data(), static_cast<int>(pcm.data.size()), pcm.rate);
  if (a.get_error() != 0 || !buffer) {
    *why = "OpenAL refused its samples";
    return 0;
  }
  g_buffers[resref] = buffer;
  return buffer;
}

std::string Hex(uint32_t id) {
  char text[16];
  snprintf(text, sizeof text, "%x", id);
  return text;
}

// The game's PlaySound at once, as the script would have played it (a voice line for its voice only).
void PlayPlain(const data::SoundCue& cue, const std::string& why) {
  if (g_plain_logged.insert(cue.sound.resref).second) {
    Log("sound player: %s plays plain (the game's PlaySound, no effects) - %s", cue.sound.resref.c_str(), why.c_str());
  }
  std::string code = "AssignCommand(StringToObject(\"" + Hex(cue.at) + "\"), PlaySound(\"" + cue.sound.resref + "\"));";
  if (!cue.sound.voice.empty()) {
    code = std::string("if (GetGender(OBJECT_SELF) == ") + (cue.sound.voice == "female" ? "GENDER_FEMALE" : "GENDER_MALE") + ") " + code;
  }
  RunScriptChunk(code.c_str(), cue.voice_of ? cue.voice_of : cue.at);
}

// Whether a voice line is for `who`'s voice (asked through the script, as the look's chunks do).
bool VoiceMatches(const data::SoundCue& cue) {
  if (cue.sound.voice.empty()) return true;
  void* who = CreatureById(cue.voice_of);
  if (!who || RunScriptChunk("SetLocalInt(OBJECT_SELF, \"n5snd_g\", GetGender(OBJECT_SELF));", cue.voice_of) == -1) return false;
  const int gender = engine::GetLocalInt(who, "n5snd_g");
  RunScriptChunk("DeleteLocalInt(OBJECT_SELF, \"n5snd_g\");", cue.voice_of);
  return (cue.sound.voice == "female") == (gender == 1);  // GENDER_MALE 0, GENDER_FEMALE 1
}

void Play(const data::SoundCue& cue) {
  if (!VoiceMatches(cue)) return;
  void* object = CreatureById(cue.at);
  if (!object) {
    Log("sound player: %s - its object 0x%08X is gone", cue.sound.resref.c_str(), cue.at);
    return;
  }
  const Al& a = LoadAl();
  if (!a.ok) return PlayPlain(cue, g_al_why);
  if (g_playing.size() >= kMostPlaying) return PlayPlain(cue, "more than 24 sounds at once");
  std::string why;
  const unsigned buffer = BufferFor(a, cue.sound.resref, &why);
  if (!buffer) return PlayPlain(cue, why);

  const float x = Field<float>(object, ee::kObjectPositionX), y = Field<float>(object, ee::kObjectPositionY);
  const float z = Field<float>(object, ee::kObjectPositionZ) + kAboveFeet;
  if (!g_first_logged) {
    g_first_logged = true;
    float listener[3] = {0, 0, 0}, facing[6] = {0, 0, 0, 0, 0, 0};
    a.get_listenerfv(AL_POSITION, listener);
    a.get_listenerfv(AL_ORIENTATION, facing);
    Log("sound player: the first sound - the game's listener at (%.1f, %.1f, %.1f) facing (%.2f, %.2f, %.2f) up (%.2f, "
        "%.2f, %.2f), distance model 0x%X; %s plays at its object's (%.1f, %.1f, %.1f) - the two should be near each other "
        "(the camera on the character)",
        listener[0], listener[1], listener[2], facing[0], facing[1], facing[2], facing[3], facing[4], facing[5],
        a.get_integer(AL_DISTANCE_MODEL), cue.sound.resref.c_str(), x, y, z);
  }

  data::SoundMix mix = data::MixFor(cue.sound);
  Playing p;
  a.get_error();
  a.gen_sources(1, &p.source);
  a.sourcei(p.source, AL_BUFFER, static_cast<int>(buffer));
  a.sourcei(p.source, AL_SOURCE_RELATIVE, 0);
  a.source3f(p.source, AL_POSITION, x, y, z);
  a.sourcef(p.source, AL_REFERENCE_DISTANCE, kReferenceMetres);
  a.sourcef(p.source, AL_MAX_DISTANCE, kMaxMetres);
  a.sourcef(p.source, AL_ROLLOFF_FACTOR, 1.0f);
  a.sourcef(p.source, AL_MAX_GAIN, std::max(1.0f, mix.gain));
  a.sourcef(p.source, AL_GAIN, mix.gain);
  a.sourcef(p.source, AL_PITCH, std::max(0.25f, std::min(4.0f, mix.pitch)));

  std::string skipped;
  if (a.efx) {
    int send = 0;
    unsigned send_filter = AL_FILTER_NULL;
    if (mix.muffle_hf < 1.0f) {  // the sends muffled too, so the echo and the reverb sound as the sound does
      unsigned f = 0;
      a.gen_filters(1, &f);
      a.filteri(f, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
      a.filterf(f, AL_LOWPASS_GAIN, 1.0f);
      a.filterf(f, AL_LOWPASS_GAINHF, mix.muffle_hf);
      p.filters.push_back(f);
      send_filter = f;
    }
    const auto add = [&](int type, float slot_gain, const char* name) -> unsigned {
      if (send >= a.sends) {
        skipped += std::string(skipped.empty() ? "" : ", ") + name;
        return 0;
      }
      unsigned effect = 0, slot = 0;
      a.gen_effects(1, &effect);
      a.effecti(effect, AL_EFFECT_TYPE, type);
      a.gen_slots(1, &slot);
      p.effects.push_back(effect);
      p.slots.push_back(slot);
      return effect;
    };
    const auto attach = [&](unsigned effect, float slot_gain) {
      const unsigned slot = p.slots.back();
      a.sloti(slot, AL_EFFECTSLOT_EFFECT, static_cast<int>(effect));
      a.slotf(slot, AL_EFFECTSLOT_GAIN, slot_gain);
      a.source3i(p.source, AL_AUXILIARY_SEND_FILTER, static_cast<int>(slot), send++, static_cast<int>(send_filter));
    };
    bool shifted = false;
    if (mix.shift != 0) {
      if (const unsigned e = add(AL_EFFECT_PITCH_SHIFTER, 1.0f, "shift")) {
        a.effecti(e, AL_PITCH_SHIFTER_COARSE_TUNE, std::max(-12, std::min(12, mix.shift)));
        attach(e, 1.0f);
        shifted = true;
      }
    }
    if (mix.echo_feedback > 0.0f) {
      if (const unsigned e = add(AL_EFFECT_ECHO, mix.echo_gain, "echo")) {
        a.effectf(e, AL_ECHO_DELAY, 0.12f);
        a.effectf(e, AL_ECHO_LRDELAY, 0.1f);
        a.effectf(e, AL_ECHO_DAMPING, 0.4f);
        a.effectf(e, AL_ECHO_FEEDBACK, mix.echo_feedback);
        attach(e, mix.echo_gain);
        p.tail = true;
      }
    }
    if (mix.reverb_gain > 0.0f) {
      if (const unsigned e = add(AL_EFFECT_REVERB, mix.reverb_gain, "reverb")) {
        a.effectf(e, AL_REVERB_DECAY_TIME, 2.5f);
        attach(e, mix.reverb_gain);
        p.tail = true;
      }
    }
    const float dry = shifted ? mix.dry : 1.0f;  // no shifter after all: the sound itself stays
    if (dry < 1.0f || mix.muffle_hf < 1.0f) {
      unsigned f = 0;
      a.gen_filters(1, &f);
      a.filteri(f, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
      a.filterf(f, AL_LOWPASS_GAIN, dry);
      a.filterf(f, AL_LOWPASS_GAINHF, mix.muffle_hf);
      a.sourcei(p.source, AL_DIRECT_FILTER, static_cast<int>(f));
      p.filters.push_back(f);
    }
  } else if (mix.shift != 0 || mix.muffle_hf < 1.0f || mix.echo_feedback > 0.0f || mix.reverb_gain > 0.0f) {
    skipped = "shift, muffle, echo and reverb (no EFX)";
  }
  a.play(p.source);
  const int error = a.get_error();
  g_playing.push_back(p);
  Log("sound player: %s at 0x%08X (%.1f, %.1f, %.1f) - pitch %.2f, shift %d, muffle %.2f, echo %.2f, reverb %.2f, volume "
      "%.2f%s%s",
      cue.sound.resref.c_str(), cue.at, x, y, z, mix.pitch, mix.shift, cue.sound.muffle, mix.echo_feedback,
      mix.reverb_gain, mix.gain, skipped.empty() ? "" : (" - skipped: " + skipped).c_str(),
      error == 0 ? "" : " - an OpenAL error");
}

// The sounds that finished (and their tails) give their source, filters, effects and slots back.
void Reap(int64_t now) {
  if (g_playing.empty()) return;
  const Al& a = g_al;
  for (auto it = g_playing.begin(); it != g_playing.end();) {
    if (!it->stopped_at) {
      int state = 0;
      a.get_sourcei(it->source, AL_SOURCE_STATE, &state);
      if (state == AL_STOPPED) it->stopped_at = now;
    }
    if (!it->stopped_at || now - it->stopped_at < (it->tail ? kTailMs : 0)) {
      ++it;
      continue;
    }
    a.stop(it->source);
    a.sourcei(it->source, AL_BUFFER, 0);
    a.delete_sources(1, &it->source);
    if (a.efx) {
      for (unsigned& s : it->slots) a.delete_slots(1, &s);
      for (unsigned& e : it->effects) a.delete_effects(1, &e);
      for (unsigned& f : it->filters) a.delete_filters(1, &f);
    }
    it = g_playing.erase(it);
  }
}

int64_t g_last_tick = -1;

void OnCreatureUpdate(void* /*creature*/) {
  if (g_pending.empty() && g_playing.empty()) return;
  const int64_t now = Now();
  if (now == g_last_tick) return;  // once a game millisecond, not once a creature
  g_last_tick = now;
  std::vector<data::SoundCue> due;
  for (auto it = g_pending.begin(); it != g_pending.end();) {
    if (now >= it->due) {
      due.push_back(it->cue);
      it = g_pending.erase(it);
    } else {
      ++it;
    }
  }
  for (const data::SoundCue& cue : due) Play(cue);
  Reap(now);
}

}  // namespace

void PlayLookSounds(const std::vector<data::SoundCue>& cues) {
  if (cues.empty()) return;
  if (!g_installed) {  // no update to time them by: the game's PlaySound, delayed, plain
    for (const data::SoundCue& cue : cues) {
      std::string code = "AssignCommand(StringToObject(\"" + Hex(cue.at) + "\"), DelayCommand(" + std::to_string(cue.in) +
                         ", PlaySound(\"" + cue.sound.resref + "\")));";
      if (!cue.sound.voice.empty()) {
        code = std::string("if (GetGender(OBJECT_SELF) == ") + (cue.sound.voice == "female" ? "GENDER_FEMALE" : "GENDER_MALE") +
               ") " + code;
      }
      RunScriptChunk(code.c_str(), cue.voice_of ? cue.voice_of : cue.at);
    }
    return;
  }
  const int64_t now = Now();
  for (const data::SoundCue& cue : cues) {
    Pending p;
    p.due = now + static_cast<int64_t>(cue.in * 1000.0f);
    p.cue = cue;
    g_pending.push_back(p);
  }
}

void InstallLookSoundPlayer() {
  if (g_installed) return;
  if (!AddCreatureUpdateListener(&OnCreatureUpdate)) {
    Log("sound player: no creature update - a look's sounds with effects play plain");
    return;
  }
  g_installed = true;
  Log("sound player: a look's sounds with effects play through OpenAL EFX (owner, 2026-11-10)");
}
