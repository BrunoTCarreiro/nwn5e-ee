#pragma once

// Conversion targets for NWN's 3e creatures.
//
// Provenance: the expected-AC-by-CR values are the 2014 DMG guideline table
// ("Monster Statistics by Challenge Rating"), NOT SRD 5.2 — personal use only,
// replaceable (docs/PLAN.md, Phase 8 / decision 9).

namespace rules {

// Expected Armor Class for a creature of this challenge rating:
// CR 0-3: 13, 4: 14, 5-7: 15, 8-9: 16, 10-12: 17, 13-16: 18, 17+: 19.
// Fractional CRs count as 0; other CRs round down.
int ExpectedMonsterArmorClass(double challenge_rating);

}  // namespace rules
