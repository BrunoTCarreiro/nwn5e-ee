#pragma once

// Cover from objects (#166 part 2; owner, 2026-11-23: "A, and apply cover to all three"): nwn5e's own sight test
// against the placeables, beside the engine's (which knows only the tileset's walls and shut doors). Each placeable's
// box is its model's visible bounds (data\placeable_bounds.txt, made at build time) at its place, turned by its facing
// and scaled (rules/cover_objects.h). cover_objects (faithful).

#include <stdint.h>

#include <string>

#include "rules/area_origin.h"
#include "rules/cover.h"
#include "rules/los_probe.h"

// The table read and the launch line (InstallPerceptionRules calls it).
void InstallCoverObjects();
// cover_objects on, the rules faithful and the table read.
bool CoverObjectsOn();

// Whether a placeable's box is in the way of the line `a` -> `b` in `area` - `self` a creature there (its placeables
// are listed through it, every 3 s at most). `blocker`: the placeable's id, `t` (optional): where the line meets it.
bool ObjectsBlockLine(void* area, uint32_t self, const float a[3], const float b[3], uint32_t* blocker, float* t);
// A placeable's resref as listed ("" when not known).
std::string ObjectName(uint32_t id);

// One body point's line: clear, or blocked by the engine's line (walls, shut doors: `blocker` kObjectInvalid) or by
// a placeable's box (`object` true, `blocker` its id).
struct CoverPoint {
  bool clear = true;
  bool object = false;
  uint32_t blocker = 0;
};
// The cover `target` has from `eye` (a creature's eyes, an effect's origin): its seven body points' lines
// (rules::ScaledBodyPoint: by its size, crouched in stealth mode), `from_id` the creature or object the lines start at, `self` a creature in the area. `by` /
// `by_id`: what blocks the most lines (a resref and its id; "the tileset" and kObjectInvalid for the engine's).
rules::Cover CoverFrom(void* area, uint32_t from_id, uint32_t self, const float eye[3], void* target,
                       CoverPoint points[rules::kLosBodyPoints], std::string* by, uint32_t* by_id);
// From `observer`'s eyes (its position + rules::EyeHeight by its size); kept a second while neither moves (nor crouches).
// None when cover_objects is off.
rules::Cover CoverBetween(void* observer, void* target, std::string* by, uint32_t* by_id);
// The words for a degree (the catalogue's SRD terms): "Half Cover" ...; "" for none.
std::string CoverWords(rules::Cover cover);

// (c) attacks: both sides told, once a round, the cover and its AC (`creature`: a creature in the way).
void TellAttackCover(void* attacker, void* target, rules::Cover cover, int bonus, uint32_t by_id, bool creature);
// Total Cover from the attacker's eyes refuses its attack (ranged anywhere, melee within reach): the attack action
// dropped before it's spent, the attacker told (action_order.cpp); an AI attacker then repositions (#166 decision 3:
// sideways for a line, else closer; melee round the obstacle), queued once a round.
bool TotalCoverHoldsAttack(void* creature, void* node);
// (c) Dexterity saves: the bonus `creature` has from cover from the effect's origin - `script_self` the running
// script's object (the caster, or an area of effect) else `versus`; a creature's sphere spell (spells.2da
// TargetShape) from its target point. Both told once a round.
int SaveCoverBonus(void* creature, uint32_t versus, uint32_t script_self, int spell);

// #166 decision 2 (owner, 2026-11-23): an area's point of origin by its shape (rules/area_origin.h) - the caster's eyes
// for a cone or a line, the face's centre for a cube from the caster, 1 m over the point aimed at for a sphere, the
// top for an area that comes down or covers the ground. nwn5e's own area spells leave out a creature behind Total
// Cover from it - no save, no effect - both told once (rules_spells2024.cpp asks for each creature in the area).
bool LeftOutOfArea(void* caster, int spell, uint32_t target);
// An area whose origin isn't the caster's or the point aimed at (Ice Knife's burst around the creature it struck):
// set while its creatures are gathered and saved.
class ScopedAreaOrigin {
 public:
  ScopedAreaOrigin(const float ground[3], rules::AreaOrigin kind, int feet);
  ~ScopedAreaOrigin();
  ScopedAreaOrigin(const ScopedAreaOrigin&) = delete;
  ScopedAreaOrigin& operator=(const ScopedAreaOrigin&) = delete;

 private:
  bool outer_;
  float outer_at_[3];
  int outer_kind_;
  int outer_feet_;
};
