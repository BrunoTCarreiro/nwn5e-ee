#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "rules/radial_cleanup.h"

// #126, the faithful radial's cleanup in the client (rules/radial_cleanup.h says what goes): radial_pages.cpp.

// The character's feats a faithful page leaves out (rules::RadialPageHidesFeat), off the client creature's two feat
// lists (its stats +0x3b0: CExoArrayList<uint16> at +0x118 and +0x128, the lists the page builders search) while the
// game builds that page - it adds a feat's entry only when it finds the feat there - and put back after. Nothing
// for a null player, or with the radial in NWN mode.
class RadialFeatsMasked {
 public:
  RadialFeatsMasked(void* player, rules::RadialPage page);
  ~RadialFeatsMasked();
  RadialFeatsMasked(const RadialFeatsMasked&) = delete;
  RadialFeatsMasked& operator=(const RadialFeatsMasked&) = delete;
  size_t Count() const { return masked_.size(); }
  std::string Feats() const;  // "28 19" - for the log

 private:
  std::vector<std::pair<uint16_t*, uint16_t>> masked_;
};

// The entries `page` always adds that the faithful radial leaves out (rules::RadialFixedEntries), taken out of
// `builder` (the page's code): each entry's icon string is referenced once there and the call that adds it follows
// within a few instructions. All or none (logged); false when none were.
bool RemoveRadialFixedEntries(rules::RadialPage page, uintptr_t builder);

// The Special Abilities and Cast Spell pages (faithful): their fixed entries taken out, Cast Spell built without the
// 3e Metamagic and Epic Spells feats. In either mode, the first rings opened logged - whose (yours, your
// associate's, another creature's, the ground's) and the slots they hold. `player_creature` is the client's own creature getter (the Special Attacks
// page's, found by radial_special_attacks.cpp, which calls this).
void InstallRadialPages(void* (*player_creature)(void* client_app));

// #126: the Spells page's own entries (Prepare Spells in slot 6) are placed - their pieces found, the spell menu hooked - so
// the class sections leave those feats out (radial_class_feats.cpp).
bool RadialSpellsEntriesPlaced();
