#include "rules/spell_slots.h"

namespace rules {

namespace {

constexpr int kClassBard = 1;
constexpr int kClassCleric = 2;
constexpr int kClassDruid = 3;
constexpr int kClassPaladin = 6;
constexpr int kClassRanger = 7;
constexpr int kClassSorcerer = 9;
constexpr int kClassWizard = 10;

// SRD 5.2: spell slots per spell level 1-9, levels 1-20.
constexpr int kFull[20][9] = {
    {2, 0, 0, 0, 0, 0, 0, 0, 0}, {3, 0, 0, 0, 0, 0, 0, 0, 0}, {4, 2, 0, 0, 0, 0, 0, 0, 0},
    {4, 3, 0, 0, 0, 0, 0, 0, 0}, {4, 3, 2, 0, 0, 0, 0, 0, 0}, {4, 3, 3, 0, 0, 0, 0, 0, 0},
    {4, 3, 3, 1, 0, 0, 0, 0, 0}, {4, 3, 3, 2, 0, 0, 0, 0, 0}, {4, 3, 3, 3, 1, 0, 0, 0, 0},
    {4, 3, 3, 3, 2, 0, 0, 0, 0}, {4, 3, 3, 3, 2, 1, 0, 0, 0}, {4, 3, 3, 3, 2, 1, 0, 0, 0},
    {4, 3, 3, 3, 2, 1, 1, 0, 0}, {4, 3, 3, 3, 2, 1, 1, 0, 0}, {4, 3, 3, 3, 2, 1, 1, 1, 0},
    {4, 3, 3, 3, 2, 1, 1, 1, 0}, {4, 3, 3, 3, 2, 1, 1, 1, 1}, {4, 3, 3, 3, 3, 1, 1, 1, 1},
    {4, 3, 3, 3, 3, 2, 1, 1, 1}, {4, 3, 3, 3, 3, 2, 2, 1, 1},
};
constexpr int kHalf[20][5] = {
    {2, 0, 0, 0, 0}, {2, 0, 0, 0, 0}, {3, 0, 0, 0, 0}, {3, 0, 0, 0, 0}, {4, 2, 0, 0, 0},
    {4, 2, 0, 0, 0}, {4, 3, 0, 0, 0}, {4, 3, 0, 0, 0}, {4, 3, 2, 0, 0}, {4, 3, 2, 0, 0},
    {4, 3, 3, 0, 0}, {4, 3, 3, 0, 0}, {4, 3, 3, 1, 0}, {4, 3, 3, 1, 0}, {4, 3, 3, 2, 0},
    {4, 3, 3, 2, 0}, {4, 3, 3, 3, 1}, {4, 3, 3, 3, 1}, {4, 3, 3, 3, 2}, {4, 3, 3, 3, 2},
};
// Prepared Spells, levels 1-20.
constexpr int kPreparedBardClericDruid[20] = {4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 16, 17, 17, 18, 18, 19, 20, 21, 22};
constexpr int kPreparedSorcerer[20] = {2, 4, 6, 7, 9, 10, 11, 12, 14, 15, 16, 16, 17, 17, 18, 18, 19, 20, 21, 22};
constexpr int kPreparedWizard[20] = {4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 16, 17, 18, 19, 21, 22, 23, 24, 25};
constexpr int kPreparedHalf[20] = {2, 3, 4, 5, 6, 6, 7, 7, 9, 9, 10, 10, 11, 11, 12, 12, 14, 14, 15, 15};

bool InRange(int level) { return level >= 1 && level <= 20; }

}  // namespace

CasterKind CasterKindOfClass(int class_row) {
  switch (class_row) {
    case kClassBard:
    case kClassCleric:
    case kClassDruid:
    case kClassSorcerer:
    case kClassWizard: return CasterKind::kFull;
    case kClassPaladin:
    case kClassRanger: return CasterKind::kHalf;
    default: return CasterKind::kNone;
  }
}

int SpellSlots2024(CasterKind kind, int level, int spell_level) {
  if (!InRange(level) || spell_level < 1) return 0;
  if (kind == CasterKind::kFull) return spell_level <= 9 ? kFull[level - 1][spell_level - 1] : 0;
  if (kind == CasterKind::kHalf) return spell_level <= 5 ? kHalf[level - 1][spell_level - 1] : 0;
  return 0;
}

int MaxSpellLevel2024(CasterKind kind, int level) {
  int highest = 0;
  for (int spell_level = 1; spell_level <= 9; ++spell_level) {
    if (SpellSlots2024(kind, level, spell_level) > 0) highest = spell_level;
  }
  return highest;
}

int CantripsKnown2024(int class_row, int class_level) {
  if (!InRange(class_level)) return 0;
  const int step = class_level >= 10 ? 2 : class_level >= 4 ? 1 : 0;
  switch (class_row) {
    case kClassBard:
    case kClassDruid: return 2 + step;
    case kClassCleric:
    case kClassWizard: return 3 + step;
    case kClassSorcerer: return 4 + step;
    default: return 0;
  }
}

int PreparedSpells2024(int class_row, int class_level) {
  if (!InRange(class_level)) return 0;
  switch (class_row) {
    case kClassBard:
    case kClassCleric:
    case kClassDruid: return kPreparedBardClericDruid[class_level - 1];
    case kClassSorcerer: return kPreparedSorcerer[class_level - 1];
    case kClassWizard: return kPreparedWizard[class_level - 1];
    case kClassPaladin:
    case kClassRanger: return kPreparedHalf[class_level - 1];
    default: return 0;
  }
}

int SpellGain2024(int class_row, int class_level, int spell_level) {
  if (spell_level == 0) return CantripsKnown2024(class_row, class_level);
  return SpellSlots2024(CasterKindOfClass(class_row), class_level, spell_level);
}

bool SpellLevelMet2024(int spell_level) { return spell_level >= 0 && spell_level <= 9; }

bool CastSpendsUse(bool faithful, int spell_level) { return !(faithful && spell_level == 0); }

bool DrawsOnSameSlots(bool memorizes, const QueuedCast& a, const QueuedCast& b) {
  if (memorizes) return a.spell == b.spell && a.metamagic == b.metamagic;
  return a.slot_level == b.slot_level;
}

bool CastQueueFull(bool faithful, int slot_level, int queued, int uses_left) {
  return faithful && CastSpendsUse(faithful, slot_level) && queued >= uses_left;
}

}  // namespace rules
