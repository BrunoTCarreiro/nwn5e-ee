#pragma once

// The look's sounds with effects, played in game (owner, 2026-11-10: "build the in-game sound player"; #154's proof:
// the game's own OpenAL Soft has EFX, SF-1..SF-6 passed). The look player hands it the cues the scripts leave out
// (data::LookCastCues / LookHitCues / LookLeapCues); each plays when due, at its object (a creature's position, a
// metre up), on its own OpenAL source with the cue's mix (data::MixFor: pitch, the pitch shifter, a low-pass, echo,
// reverb, volume) - the bench's preview, live, no new audio files.
//
// When it can't - no OpenAL or no EFX, a sound that isn't PCM / IMA ADPCM (an MP3 one), too many at once, no update
// hook - the cue plays plain through the game's PlaySound at the same moment, and the log says why (once per sound).
// The first play logs where the game's listener is and where the sound went, so a first test shows the coordinates
// agree (the game's world coordinates are taken as its OpenAL's).

#include <vector>

#include "data/anim_look_sound.h"

void PlayLookSounds(const std::vector<data::SoundCue>& cues);

// Registers its creature-update listener (InstallAnimLayers calls it); logged if it can't.
void InstallLookSoundPlayer();
