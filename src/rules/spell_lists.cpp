#include "rules/spell_lists.h"

#include <algorithm>

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

}  // namespace

ListChange LongRestChange(int class_row) {
  switch (class_row) {
    case kClassCleric:
    case kClassDruid:
    case kClassWizard: return ListChange::kAny;
    case kClassPaladin:
    case kClassRanger: return ListChange::kOne;
    case kClassBard:
    case kClassSorcerer:
    default: return ListChange::kNone;
  }
}

ListChange LongRestCantripChange(int class_row) {
  return class_row == kClassWizard ? ListChange::kOne : ListChange::kNone;
}

bool PreparesFromSpellbook(int class_row) { return class_row == kClassWizard; }

int SpellbookSpells2024(int wizard_level) {
  if (wizard_level < 1) return 0;
  return 6 + 2 * (std::min(wizard_level, 20) - 1);
}

int KnownSpellsCap2024(int class_row, int class_level, int spell_level) {
  if (CasterKindOfClass(class_row) == CasterKind::kNone) return -1;
  if (spell_level == 0) return CantripsKnown2024(class_row, class_level);
  if (spell_level < 1 || spell_level > 9 || SpellGain2024(class_row, class_level, spell_level) <= 0) return 0;
  if (class_row == kClassWizard) return SpellbookSpells2024(class_level);
  return PreparedSpells2024(class_row, class_level);
}

bool ListChangeAllowed(ListChange change, const std::vector<int>& before, const std::vector<int>& after, int limit) {
  if (static_cast<int>(after.size()) > limit) return false;
  if (static_cast<int>(before.size()) > limit) return true;
  int gone = 0;
  for (int spell : before) {
    if (std::find(after.begin(), after.end(), spell) == after.end()) ++gone;
  }
  switch (change) {
    case ListChange::kAny: return true;
    case ListChange::kOne: return gone <= 1;
    case ListChange::kNone:
    default: return gone == 0;
  }
}

int SpellsLeftOnPage(int level_left, int prepared_total, int known_total) {
  return std::max(0, std::min(level_left, prepared_total - known_total));
}

bool RemoveIconShown(bool picked_this_level_up, bool swap_left) { return picked_this_level_up || swap_left; }

bool AddIconShown(int spells_left) { return spells_left > 0; }

std::vector<int> WizardPageRows(const std::vector<int>& book, const std::vector<int>& known) {
  std::vector<int> rows;
  for (const std::vector<int>* list : {&book, &known}) {
    for (int spell : *list) {
      if (std::find(rows.begin(), rows.end(), spell) == rows.end()) rows.push_back(spell);
    }
  }
  return rows;
}

bool LevelUpSwapAllowed(int class_row, bool cantrip, int swaps_done) {
  if (swaps_done >= 1) return false;
  switch (class_row) {
    case kClassBard:
    case kClassSorcerer: return true;  // a spell or a cantrip
    case kClassCleric:
    case kClassDruid: return cantrip;  // a cantrip only (their prepared spells change after a Long Rest)
    default: return false;
  }
}

WizardLists TakeWizardPicks(const std::vector<int>& book, const std::vector<int>& known, int prepared_limit) {
  WizardLists lists;
  lists.book = book;
  for (int spell : known) {
    if (std::find(book.begin(), book.end(), spell) != book.end() &&
        static_cast<int>(lists.prepared.size()) < prepared_limit) {
      lists.prepared.push_back(spell);  // prepared before
    }
  }
  for (int spell : known) {
    if (std::find(book.begin(), book.end(), spell) != book.end()) continue;
    lists.book.push_back(spell);  // a pick: into the book
    if (static_cast<int>(lists.prepared.size()) < prepared_limit) lists.prepared.push_back(spell);
  }
  return lists;
}

int BoundedKnownCap(int cap, int known_here, int prepared_total, int known_total) {
  return std::min(cap, known_here + std::max(0, prepared_total - known_total));
}

}  // namespace rules
