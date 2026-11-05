// #36 Part 3a (owner, 2026-11-04: "go with your recommendations. But for 3, can you make the window show up for wizard
// characters while they are resting? Here they can change their memorized spells but the button to accept the
// memorized spells only becomes enabled once the full rest completes?" - 1A 2A 3A; docs/PLAN.md "Phase 4"),
// spellcasting faithful: the Prepare spells window and the lists behind it.
// - A 2024 caster's known list (per class slot) is its cantrips (level 0) and prepared spells (1+): the radial and the
//   spellbook show those and casting takes any of them with any slot of its level (classes.if_spells2024;
//   rules_spell_slots.cpp caps the list at 2024's counts). A Wizard's whole spellbook is its Spellbook item - a book,
//   undroppable, the spell ids in its local n5_spellbook (copied on the character as nwn5e_spellbook) - and a scroll
//   it learns goes into the book.
// - The window (NUI n5prep): the class's cantrips, its prepared spells (from its class's list, or the Wizard's book)
//   and the Wizard's spellbook; a click picks or drops a spell and shows its description. It opens when a rest begins
//   (its Accept waits for the rest's end; an interrupted rest closes it, nothing changed - and a rest's start ends
//   an earlier rest's change, 0.75.2), from the radial (feat Prepare Spells)
//   and by itself when a list is empty or over its count (a new character, 0.75.0's change). What may change
//   (rules/spell_lists.h, SRD 5.2): filling a list up, always; as a Long Rest finishes - in the window of that rest,
//   until it is accepted, cancelled or closed (0.75.4: no grace period after it) - Cleric, Druid and Wizard any
//   prepared spell, Paladin and Ranger one, the Wizard one cantrip; the spellbook only grows. A rest with no window
//   at its start (ForceRest: a script's, !test's) opens one at its end.
// - Creatures that aren't player characters (henchmen too) cast as in Part 1: where a caster class's known list is
//   empty at a level it has slots for, it holds every spell of its class at that level (filled when first seen and
//   after a rest). NWN keeps no list for a Cleric, Druid, Paladin or Ranger, and a blueprint Wizard has only its
//   memorised spells (nw_skelmage), so without it they would cast nothing; a Bard's or Sorcerer's list stays.
// Part 3b: the Spells page at creation and level-up (with the level-up swaps: Bard and Sorcerer a spell, a cantrip).

#include "prepare_spells.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "nui.h"
#include "rest.h"
#include "script_hooks.h"
#include "rules/class_features.h"
#include "rules/nwn_conversion.h"
#include "rules/spell_lists.h"
#include "rules/spell_slots.h"

namespace {

constexpr const char* kWindow = "n5prep";
constexpr const char* kBookResRef = "nw_it_book015";  // a plain book (base item 74), renamed
constexpr const char* kBookTag = "n5_spellbook";
constexpr const char* kBookVar = "n5_spellbook";       // on the book: the spell ids, "107,166,..."
constexpr const char* kBookCopy = "nwn5e_spellbook";   // the same on the character
constexpr const char* kBookChecked = "nwn5e_spellbook_checked";  // 1 a book found, 2 none
constexpr const char* kChangeVar = "nwn5e_prepare_change";      // 1 while a Long Rest's change is open
constexpr const char* kBaseVar = "nwn5e_prepare_base";          // + slot: "c1,2/p3,4" when it opened
constexpr int kTabCantrips = 0;
constexpr int kTabPrepared = 1;
constexpr int kTabBook = 2;
constexpr int kNwnClassWizard = 10;

using AddKnownFn = void (*)(void* class_info, uint8_t level, uint32_t spell);
using RemoveKnownFn = void (*)(void* class_info, uint8_t level, uint32_t spell);
using GetKnownFn = uint32_t (*)(void* class_info, uint8_t level, uint8_t index);
using NumberKnownFn = uint16_t (*)(void* class_info, uint8_t level);
using GetSpellFn = void* (*)(const void* spells, int id);
using SpellLevelFn = uint8_t (*)(void* spell, uint8_t nwn_class);
using SpellNameFn = void* (*)(void* spell, void* out);  // CExoString CNWSpell::GetSpellNameText()
using ExoStringFreeFn = void (*)(void* text);           // CExoString::~CExoString
using LearnScrollFn = int (*)(void* creature, uint32_t item);

AddKnownFn g_add_known = nullptr;
RemoveKnownFn g_remove_known = nullptr;
GetKnownFn g_get_known = nullptr;
NumberKnownFn g_number_known = nullptr;
GetSpellFn g_get_spell = nullptr;
SpellLevelFn g_spell_level = nullptr;
SpellNameFn g_spell_name = nullptr;
ExoStringFreeFn g_free_text = nullptr;
LearnScrollFn g_learn_scroll = nullptr;
int g_lines = 0;

bool LogLine() { return g_lines < 300 && ++g_lines; }

struct ExoText {
  char* text = nullptr;
  uint32_t length = 0;
};

struct SpellEntry {
  int id = 0;
  int level = 0;
  std::string name;
};

// --- The engine's lists ------------------------------------------------------------------------------------

void* RulesObject() {
  static void** const global = reinterpret_cast<void**>(EngineExport(ee::kRules));
  return global ? *global : nullptr;
}

void* ClassInfoOf(void* stats, int slot) {
  return static_cast<uint8_t*>(stats) + ee::kStatsClassInfo + slot * ee::kClassInfoSize;
}

bool IsPlayer(void* creature) { return creature && Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0; }

uint32_t IdOf(void* creature) { return Field<uint32_t>(creature, ee::kObjectId); }

std::vector<int> Known(void* stats, int slot, int level) {
  std::vector<int> spells;
  void* info = ClassInfoOf(stats, slot);
  const int count = g_number_known(info, static_cast<uint8_t>(level));
  for (int i = 0; i < count && i < 255; ++i) {
    spells.push_back(static_cast<int>(g_get_known(info, static_cast<uint8_t>(level), static_cast<uint8_t>(i))));
  }
  return spells;
}

std::vector<int> KnownFrom(void* stats, int slot, int first_level, int last_level) {
  std::vector<int> spells;
  for (int level = first_level; level <= last_level; ++level) {
    for (int spell : Known(stats, slot, level)) spells.push_back(spell);
  }
  return spells;
}

// Every spell with a level for the class (0-9), by level then name; built once per class.
const std::vector<SpellEntry>& ClassSpells(int row) {
  static std::map<int, std::vector<SpellEntry>> catalogue;
  auto found = catalogue.find(row);
  if (found != catalogue.end()) return found->second;
  std::vector<SpellEntry>& entries = catalogue[row];
  void* rules_object = RulesObject();
  void* spells = rules_object ? Field<void*>(rules_object, ee::kRulesSpells) : nullptr;
  const int count = spells ? Field<int32_t>(spells, ee::kSpellArrayCount) : 0;
  for (int id = 0; id < count && count < 100000; ++id) {
    void* spell = g_get_spell(spells, id);
    if (!spell) continue;
    const int level = g_spell_level(spell, static_cast<uint8_t>(row));
    if (level > 9) continue;
    ExoText text;
    g_spell_name(spell, &text);
    std::string name = text.text ? text.text : "";
    g_free_text(&text);
    if (name.empty()) continue;
    entries.push_back({id, level, name});
  }
  std::sort(entries.begin(), entries.end(), [](const SpellEntry& a, const SpellEntry& b) {
    return a.level != b.level ? a.level < b.level : a.name < b.name;
  });
  return entries;
}

const SpellEntry* EntryOf(int row, int id) {
  for (const SpellEntry& e : ClassSpells(row)) {
    if (e.id == id) return &e;
  }
  return nullptr;
}

// The highest spell level the class has slots for at its level (0: cantrips only).
int TopLevel(int row, int level) {
  int top = 0;
  for (int spell_level = 1; spell_level <= 9; ++spell_level) {
    if (rules::SpellGain2024(row, level, spell_level) > 0) top = spell_level;
  }
  return top;
}

bool Contains(const std::vector<int>& list, int id) { return std::find(list.begin(), list.end(), id) != list.end(); }

std::string Join(const std::vector<int>& list) {
  std::string text;
  for (int id : list) text += (text.empty() ? "" : ",") + std::to_string(id);
  return text;
}

std::vector<int> Split(const std::string& text) {
  std::vector<int> list;
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = text.find(',', at);
    const std::string piece = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
    if (!piece.empty()) list.push_back(atoi(piece.c_str()));
    if (end == std::string::npos) break;
    at = end + 1;
  }
  return list;
}

std::string Quoted(const std::string& text) {  // an NWScript string literal
  std::string out = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') out += '\\';
    if (c == '\n' || c == '\r') {
      out += ' ';
      continue;
    }
    out += c;
  }
  return out + "\"";
}

// --- The Wizard's Spellbook item ---------------------------------------------------------------------------------

std::vector<int> Book(void* creature) { return Split(engine::GetLocalString(creature, kBookCopy)); }

void WriteBook(void* creature, const std::vector<int>& book) {
  std::string listing;
  for (int level = 1; level <= 9; ++level) {
    std::string names;
    for (const SpellEntry& e : ClassSpells(kNwnClassWizard)) {
      if (e.level == level && Contains(book, e.id)) names += (names.empty() ? "" : ", ") + e.name;
    }
    if (!names.empty()) listing += "Level " + std::to_string(level) + ": " + names + ". ";
  }
  const std::string description =
      "Your spellbook: the level 1+ Wizard spells you know. After a Long Rest you prepare spells from it (Prepare "
      "Spells, on the radial). " + listing;
  std::string code =
      "object b = GetItemPossessedBy(OBJECT_SELF, \"" + std::string(kBookTag) + "\");\n"
      "if (!GetIsObjectValid(b)) {\n"
      "  b = CreateItemOnObject(\"" + std::string(kBookResRef) + "\", OBJECT_SELF, 1, \"" + kBookTag + "\");\n"
      "  SetName(b, \"Spellbook\"); SetIdentified(b, TRUE); SetItemCursedFlag(b, TRUE); SetPlotFlag(b, TRUE);\n"
      "}\n"
      "SetLocalString(b, \"" + kBookVar + "\", \"" + Join(book) + "\");\n"
      "SetLocalString(OBJECT_SELF, \"" + kBookCopy + "\", \"" + Join(book) + "\");\n"
      "SetDescription(b, " + Quoted(description) + ");\n";
  RunScriptChunk(code.c_str(), IdOf(creature));
}

// The book found on the character (its list copied on the character); false when it has none.
bool FindBook(void* creature) {
  std::string code =
      "object b = GetItemPossessedBy(OBJECT_SELF, \"" + std::string(kBookTag) + "\");\n"
      "if (GetIsObjectValid(b)) SetLocalString(OBJECT_SELF, \"" + kBookCopy + "\", GetLocalString(b, \"" + kBookVar +
      "\"));\n"
      "SetLocalInt(OBJECT_SELF, \"" + kBookChecked + "\", GetIsObjectValid(b) ? 1 : 2);\n";
  RunScriptChunk(code.c_str(), IdOf(creature));
  return engine::GetLocalInt(creature, kBookChecked) == 1;
}

// --- Who prepares what -------------------------------------------------------------------------------------------

struct CasterSlot {
  int slot = 0;
  int row = 0;
  int level = 0;
};

std::vector<CasterSlot> CasterSlots(void* stats) {
  std::vector<CasterSlot> casters;
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  for (size_t i = 0; i < slots.size(); ++i) {
    if (rules::CasterKindOfClass(slots[i].nwn_class_id) == rules::CasterKind::kNone) continue;
    casters.push_back({static_cast<int>(i), slots[i].nwn_class_id, slots[i].levels});
  }
  return casters;
}

bool ChangeOpen(void* creature) { return engine::GetLocalInt(creature, kChangeVar) != 0; }

void SetChangeOpen(void* creature, bool open) {
  if (ChangeOpen(creature) == open) return;
  engine::SetLocalInt(creature, kChangeVar, open ? 1 : 0);
}

// The lists when a Long Rest's change opened ("c1,2/p3,4"), per class slot.
void SaveBase(void* creature, void* stats, const CasterSlot& c) {
  const std::string value = "c" + Join(Known(stats, c.slot, 0)) + "/p" + Join(KnownFrom(stats, c.slot, 1, 9));
  std::string code = "SetLocalString(OBJECT_SELF, \"" + std::string(kBaseVar) + std::to_string(c.slot) + "\", \"" +
                     value + "\");";
  RunScriptChunk(code.c_str(), IdOf(creature));
}

void LoadBase(void* creature, int slot, std::vector<int>* cantrips, std::vector<int>* prepared) {
  const std::string value = engine::GetLocalString(creature, (std::string(kBaseVar) + std::to_string(slot)).c_str());
  const size_t split = value.find("/p");
  if (value.size() < 1 || value[0] != 'c' || split == std::string::npos) return;
  *cantrips = Split(value.substr(1, split - 1));
  *prepared = Split(value.substr(split + 2));
}

// --- The window ----------------------------------------------------------------------------------------------------

struct Session {
  CasterSlot caster;
  int tab = kTabPrepared;
  std::vector<int> cantrips, prepared, book;           // being edited
  std::vector<int> base_cantrips, base_prepared;       // what may go: the lists when the change opened
  std::vector<int> start_book;                         // the book when the window opened (it only grows)
  std::vector<int> shown;                              // the tab's spells, in the list's order
  bool resting = false;                                // opened as a rest began: Accept waits for its end
  bool rest_done = false;
  int selected = -1;
  std::string note;                                    // the last click's word
  int token = 0;                                       // its window (a replaced window's events aren't its)
};
std::map<uint32_t, Session> g_sessions;
// Opened on the next update - not from inside the engine's rest or a NUI event: the class slot (-1 the first),
// whether a rest has begun.
struct PendingOpen {
  int slot = -1;
  bool resting = false;
};
std::map<uint32_t, PendingOpen> g_open_pending;
constexpr const char* kSeenVar = "nwn5e_prepare_seen";  // on a creature: its lists looked at (saved with it)

int CantripLimit(const Session& s) { return rules::CantripsKnown2024(s.caster.row, s.caster.level); }
int PreparedLimit(const Session& s) { return rules::PreparedSpells2024(s.caster.row, s.caster.level); }
int BookLimit(const Session& s) {
  return s.caster.row == kNwnClassWizard ? std::max(rules::SpellbookSpells2024(s.caster.level),
                                                    static_cast<int>(s.start_book.size()))
                                         : 0;
}

rules::ListChange PreparedChange(void* creature, const Session& s) {
  return ChangeOpen(creature) || s.resting ? rules::LongRestChange(s.caster.row)
                                                                : rules::ListChange::kNone;
}

rules::ListChange CantripChange(void* creature, const Session& s) {
  return ChangeOpen(creature) || s.resting ? rules::LongRestCantripChange(s.caster.row)
                                                                : rules::ListChange::kNone;
}

bool Waiting(const Session& s) { return s.resting && !s.rest_done; }

bool Allowed(void* creature, const Session& s) {
  return rules::ListChangeAllowed(CantripChange(creature, s), s.base_cantrips, s.cantrips, CantripLimit(s)) &&
         rules::ListChangeAllowed(PreparedChange(creature, s), s.base_prepared, s.prepared, PreparedLimit(s)) &&
         rules::ListChangeAllowed(rules::ListChange::kNone, s.start_book, s.book, BookLimit(s));
}

const char* ClassName(int row) {
  switch (row) {
    case 1: return "Bard";
    case 2: return "Cleric";
    case 3: return "Druid";
    case 6: return "Paladin";
    case 7: return "Ranger";
    case 9: return "Sorcerer";
    case 10: return "Wizard";
    default: return "Caster";
  }
}

const char* ChangeWords(rules::ListChange change) {
  switch (change) {
    case rules::ListChange::kAny: return "any of them";
    case rules::ListChange::kOne: return "one of them";
    case rules::ListChange::kNone:
    default: return "none (fill empty places only)";
  }
}

std::string Layout(bool wizard, bool several) {
  using namespace nui;
  const std::string icon =
      "{\"type\":\"button_image\",\"label\":{\"bind\":\"ic\"},\"value\":null,\"id\":\"pi\",\"width\":32.0,"
      "\"height\":32.0}";
  const std::string name =
      "{\"type\":\"button\",\"label\":{\"bind\":\"nm\"},\"value\":null,\"id\":\"sp\",\"height\":32.0}";
  const std::string level = "{\"type\":\"label\",\"label\":null,\"value\":{\"bind\":\"lv\"},\"text_halign\":1,"
                            "\"text_valign\":1}";
  const std::string mark = "{\"type\":\"label\",\"label\":null,\"value\":{\"bind\":\"mk\"},\"text_halign\":1,"
                           "\"text_valign\":1}";
  const std::string list = ListView(Cell(icon, 32.0f, false) + "," + Cell(name, 0.0f, true) + "," +
                                        Cell(level, 70.0f, false) + "," + Cell(mark, 90.0f, false),
                                    Bind("n"), 34.0f);
  // The Wizard's Spellbook first: the book is what it prepares from (owner, 2026-11-05: "PR6. spellbook tab should
  // be the first one.").
  std::string tabs = wizard ? Sized(Button("t2", Str("Spellbook")), 140.0f) + "," : std::string();
  tabs += Sized(Button("t0", Str("Cantrips")), 140.0f) + "," + Sized(Button("t1", Str("Prepared")), 140.0f);
  if (several) tabs += "," + Spacer() + "," + Sized(Button("cls", Str("Next class")), 140.0f);
  const std::string ok =
      "{\"type\":\"button\",\"label\":\"Accept\",\"value\":null,\"id\":\"ok\",\"width\":120.0,"
      "\"enabled\":{\"bind\":\"ok_on\"}}";
  const std::string root =
      Col(Label(Bind("head"), 26.0f) + "," + Label(Bind("counts"), 26.0f) + "," + Row(tabs) + "," +
          Label(Bind("tab"), 22.0f) + "," +
          Row(Sized(list, 480.0f) + "," + Text(Bind("desc"))) + "," + Text(Bind("status"), 56.0f) + "," +
          Row(Spacer() + "," + ok + "," + Sized(Button("cancel", Str("Cancel")), 120.0f)));
  return Window(root, Str("Prepare spells"), 100.0f, 70.0f, 980.0f, 680.0f);
}

std::string JsonString(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

// The tab's spells and the window's texts.
void Sync(uint32_t player) {
  auto found = g_sessions.find(player);
  void* creature = CreatureById(player);
  if (found == g_sessions.end() || !creature) return;
  Session& s = found->second;
  const int row = s.caster.row;
  const int top = TopLevel(row, s.caster.level);
  s.shown.clear();
  const std::vector<int>* picked = &s.prepared;
  if (s.tab == kTabCantrips) {
    picked = &s.cantrips;
    for (const SpellEntry& e : ClassSpells(row)) {
      if (e.level == 0) s.shown.push_back(e.id);
    }
  } else if (s.tab == kTabBook) {
    picked = &s.book;
    for (const SpellEntry& e : ClassSpells(row)) {
      if (e.level >= 1 && e.level <= top) s.shown.push_back(e.id);
    }
  } else {
    for (const SpellEntry& e : ClassSpells(row)) {
      if (e.level < 1 || e.level > top) continue;
      if (rules::PreparesFromSpellbook(row) && !Contains(s.book, e.id)) continue;
      s.shown.push_back(e.id);
    }
  }
  std::string names = "[", levels = "[", marks = "[";  // each under the script literals' 8 KB cap
  for (size_t i = 0; i < s.shown.size(); ++i) {
    const SpellEntry* e = EntryOf(row, s.shown[i]);
    const bool on = Contains(*picked, s.shown[i]);
    const char* sep = i ? "," : "";
    names += sep + JsonString(e ? e->name : "?");
    levels += sep + JsonString(e && e->level ? "Level " + std::to_string(e->level) : "Cantrip");
    marks += sep + JsonString(on ? (s.tab == kTabBook ? "in book" : "prepared") : "");
  }
  names += "]";
  levels += "]";
  marks += "]";
  NuiSetJson(player, kWindow, "nm", names);
  NuiSetJson(player, kWindow, "lv", levels);
  NuiSetJson(player, kWindow, "mk", marks);
  NuiSetJson(player, kWindow, "n", std::to_string(s.shown.size()));
  // The icons: spells.2da's IconResRef, read by the script.
  const int token = NuiToken(player, kWindow);
  std::string code = "json ids = JsonParse(\"[" + Join(s.shown) +
                     "]\"); json icons = JsonArray(); int i; int n = JsonGetLength(ids);\n"
                     "for (i = 0; i < n; i++) icons = JsonArrayInsert(icons, JsonString(Get2DAString(\"spells\", "
                     "\"IconResRef\", JsonGetInt(JsonArrayGet(ids, i)))));\n"
                     "NuiSetBind(OBJECT_SELF, " + std::to_string(token) + ", \"ic\", icons);";
  RunScriptChunk(code.c_str(), player);
  // Two lines (0.75.6: one was cut off past "spellbook 10 of"): the class, then the counts. The tab's hint is a
  // one-line label (kept short); the status wraps in its text box.
  const std::string head = std::string(ClassName(row)) + " " + std::to_string(s.caster.level);
  std::string counts = "Cantrips " + std::to_string(s.cantrips.size()) + " of " + std::to_string(CantripLimit(s)) +
                       "    Prepared " + std::to_string(s.prepared.size()) + " of " +
                       std::to_string(PreparedLimit(s));
  if (row == kNwnClassWizard) {
    counts += "    Spellbook " + std::to_string(s.book.size()) + " of " + std::to_string(BookLimit(s));
  }
  const char* tab = s.tab == kTabCantrips ? "Cantrips you know - click one to pick or drop it"
                    : s.tab == kTabBook   ? "Spellbook - click a spell to add it (it stays in the book)"
                    : rules::PreparesFromSpellbook(row) ? "Prepared spells - from your spellbook"
                                                        : "Prepared spells - from your class's list";
  std::string status;
  if (Waiting(s)) {
    status = "Resting. You can change your spells now; Accept is ready once the rest is complete.";
  } else if (ChangeOpen(creature)) {
    status = std::string("After your Long Rest you can change prepared spells: ") +
             ChangeWords(rules::LongRestChange(row)) +
             (rules::LongRestCantripChange(row) != rules::ListChange::kNone ? "; and one cantrip" : "") +
             " - in this window; once you accept, cancel or close it, the change is over until your next Long "
             "Rest.";
  } else {
    status = "Fill empty places now; prepared spells change after a Long Rest.";
  }
  if (!s.note.empty()) status = s.note + " " + status;
  const bool ok = !Waiting(s) && Allowed(creature, s);
  if (!Waiting(s) && !Allowed(creature, s)) status += " Too many changes for this rest - undo some to accept.";
  NuiSetTexts(player, kWindow, {{"head", head}, {"counts", counts}, {"tab", tab}, {"status", status}});
  NuiSetJson(player, kWindow, "ok_on", ok ? "true" : "false");
}

void ShowDescription(uint32_t player, int spell) {
  const int token = NuiToken(player, kWindow);
  if (!token) return;
  std::string code = "NuiSetBind(OBJECT_SELF, " + std::to_string(token) +
                     ", \"desc\", JsonString(GetStringByStrRef(StringToInt(Get2DAString(\"spells\", \"SpellDesc\", " +
                     std::to_string(spell) + ")))));";
  RunScriptChunk(code.c_str(), player);
}

void Open(uint32_t player, int slot, bool resting) {
  void* creature = CreatureById(player);
  void* stats = StatsOf(creature);
  if (!stats) return;
  const std::vector<CasterSlot> casters = CasterSlots(stats);
  if (casters.empty()) {
    TellPlayer(player, "nwn5e: Prepare spells - you have no spellcasting class.");
    return;
  }
  const CasterSlot* caster = &casters[0];
  for (const CasterSlot& c : casters) {
    if (c.slot == slot) caster = &c;
  }
  Session s;
  s.caster = *caster;
  s.resting = resting;
  s.cantrips = Known(stats, s.caster.slot, 0);
  s.prepared = KnownFrom(stats, s.caster.slot, 1, 9);
  if (s.caster.row == kNwnClassWizard) s.book = s.start_book = Book(creature);
  s.base_cantrips = s.cantrips;
  s.base_prepared = s.prepared;
  if (ChangeOpen(creature)) LoadBase(creature, s.caster.slot, &s.base_cantrips, &s.base_prepared);
  if (s.prepared.empty() && !s.cantrips.empty() && CantripLimit(s) > 0) s.tab = kTabPrepared;
  if (s.cantrips.empty() && CantripLimit(s) > 0) s.tab = kTabCantrips;
  if (s.caster.row == kNwnClassWizard && static_cast<int>(s.book.size()) < BookLimit(s)) s.tab = kTabBook;
  g_sessions[player] = s;
  const int token = NuiOpen(player, kWindow, Layout(s.caster.row == kNwnClassWizard, casters.size() > 1));
  if (!token) {
    g_sessions.erase(player);
    return;
  }
  g_sessions[player].token = token;
  Sync(player);
  if (LogLine()) {
    Log("prepare: 0x%08X window opened - %s %d%s, cantrips %zu, prepared %zu%s", player, ClassName(s.caster.row),
        s.caster.level, resting ? " (a rest began)" : "", s.cantrips.size(), s.prepared.size(),
        ChangeOpen(creature) ? ", a Long Rest's change open" : "");
  }
}

// Accept: the lists into the engine (and the book).
void Commit(uint32_t player) {
  auto found = g_sessions.find(player);
  void* creature = CreatureById(player);
  void* stats = StatsOf(creature);
  if (found == g_sessions.end() || !stats) return;
  const Session& s = found->second;
  void* info = ClassInfoOf(stats, s.caster.slot);
  const std::vector<int> cantrips = Known(stats, s.caster.slot, 0);
  const std::vector<int> prepared = KnownFrom(stats, s.caster.slot, 1, 9);
  int removed = 0, added = 0;
  for (int id : cantrips) {
    if (!Contains(s.cantrips, id)) g_remove_known(info, 0, static_cast<uint32_t>(id)), ++removed;
  }
  for (int id : s.cantrips) {
    if (!Contains(cantrips, id)) g_add_known(info, 0, static_cast<uint32_t>(id)), ++added;
  }
  for (int id : prepared) {
    const SpellEntry* e = EntryOf(s.caster.row, id);
    if (e && !Contains(s.prepared, id)) g_remove_known(info, static_cast<uint8_t>(e->level), static_cast<uint32_t>(id)), ++removed;
  }
  for (int id : s.prepared) {
    const SpellEntry* e = EntryOf(s.caster.row, id);
    if (e && !Contains(prepared, id)) g_add_known(info, static_cast<uint8_t>(e->level), static_cast<uint32_t>(id)), ++added;
  }
  if (s.caster.row == kNwnClassWizard && s.book != s.start_book) WriteBook(creature, s.book);
  if (LogLine()) {
    Log("prepare: 0x%08X accepted - %s: cantrips %zu, prepared %zu (+%d -%d)%s", player, ClassName(s.caster.row),
        s.cantrips.size(), s.prepared.size(), added, removed,
        s.book != s.start_book ? (", spellbook " + std::to_string(s.book.size())).c_str() : "");
  }
}

void Toggle(uint32_t player, int index) {
  auto found = g_sessions.find(player);
  void* creature = CreatureById(player);
  if (found == g_sessions.end() || !creature) return;
  Session& s = found->second;
  if (index < 0 || index >= static_cast<int>(s.shown.size())) return;
  const int id = s.shown[index];
  s.selected = id;
  s.note.clear();
  std::vector<int>* list = s.tab == kTabCantrips ? &s.cantrips : s.tab == kTabBook ? &s.book : &s.prepared;
  const int limit = s.tab == kTabCantrips ? CantripLimit(s) : s.tab == kTabBook ? BookLimit(s) : PreparedLimit(s);
  auto at = std::find(list->begin(), list->end(), id);
  if (at != list->end()) {
    if (s.tab == kTabBook && Contains(s.start_book, id)) {
      s.note = "A spell stays in your spellbook.";
    } else {
      list->erase(at);
      if (s.tab == kTabBook) s.prepared.erase(std::remove(s.prepared.begin(), s.prepared.end(), id), s.prepared.end());
    }
  } else if (static_cast<int>(list->size()) >= limit) {
    s.note = "That list is full - drop a spell first.";
  } else {
    list->push_back(id);
  }
  Sync(player);
  ShowDescription(player, id);
}

// The window closed (Accept, Cancel, its X): a Long Rest's change is over (0.75.4).
void EndSession(uint32_t player, const char* how) {
  g_sessions.erase(player);
  void* creature = CreatureById(player);
  if (creature && ChangeOpen(creature)) {
    SetChangeOpen(creature, false);
    if (LogLine()) Log("prepare: 0x%08X window %s - the Long Rest's change is over", player, how);
  }
}

void OnEvent(const NuiEvent& event) {
  auto found = g_sessions.find(event.player);
  if (event.type == "close") {
    if (found != g_sessions.end() && found->second.token == event.token) EndSession(event.player, "closed");
    return;
  }
  if (event.type != "click" || found == g_sessions.end()) return;
  Session& s = found->second;
  if (event.element == "sp" || event.element == "pi") {
    Toggle(event.player, event.array_index);
  } else if (event.element == "t0" || event.element == "t1" || event.element == "t2") {
    s.tab = event.element[1] - '0';
    s.note.clear();
    Sync(event.player);
  } else if (event.element == "cls") {
    void* stats = StatsOf(CreatureById(event.player));
    const std::vector<CasterSlot> casters = stats ? CasterSlots(stats) : std::vector<CasterSlot>();
    int next = casters.empty() ? 0 : casters[0].slot;
    for (size_t i = 0; i < casters.size(); ++i) {
      if (casters[i].slot == s.caster.slot && i + 1 < casters.size()) next = casters[i + 1].slot;
    }
    g_open_pending[event.player] = {next, Waiting(s)};  // NuiOpen replaces the window on the next update
    g_sessions.erase(event.player);
  } else if (event.element == "ok") {
    void* creature = CreatureById(event.player);
    if (Waiting(s) || !creature || !Allowed(creature, s)) return;
    Commit(event.player);
    EndSession(event.player, "accepted");
    NuiClose(event.player, kWindow);
  } else if (event.element == "cancel") {
    EndSession(event.player, "cancelled");
    NuiClose(event.player, kWindow);
  }
}

// --- When it opens -----------------------------------------------------------------------------------------------

bool HasLongRestChange(void* stats) {
  for (const CasterSlot& c : CasterSlots(stats)) {
    if (rules::LongRestChange(c.row) != rules::ListChange::kNone ||
        rules::LongRestCantripChange(c.row) != rules::ListChange::kNone) {
      return true;
    }
  }
  return false;
}

int FirstLongRestSlot(void* stats) {
  for (const CasterSlot& c : CasterSlots(stats)) {
    if (rules::LongRestChange(c.row) != rules::ListChange::kNone) return c.slot;
  }
  return -1;
}

void RestBegun(void* creature) {
  if (!IsPlayer(creature) || !HasLongRestChange(StatsOf(creature))) return;
  SetChangeOpen(creature, false);  // 0.75.2: an earlier rest's change ends - only this rest's end opens one
  const uint32_t id = IdOf(creature);
  auto found = g_sessions.find(id);
  if (found != g_sessions.end()) {  // open already: it waits for the rest's end now
    found->second.resting = true;
    found->second.rest_done = false;
    Sync(id);
    return;
  }
  g_open_pending[id] = {FirstLongRestSlot(StatsOf(creature)), true};
}

void RestDone(void* stats) {
  void* creature = CreatureOf(stats);
  if (!creature || !IsPlayer(creature) || !HasLongRestChange(stats)) return;
  for (const CasterSlot& c : CasterSlots(stats)) SaveBase(creature, stats, c);
  SetChangeOpen(creature, true);
  const uint32_t id = IdOf(creature);
  auto found = g_sessions.find(id);
  if (found != g_sessions.end()) {
    found->second.rest_done = true;
    found->second.note = "The rest is complete.";
    LoadBase(creature, found->second.caster.slot, &found->second.base_cantrips, &found->second.base_prepared);
    Sync(id);
  } else {
    g_open_pending[id] = {FirstLongRestSlot(stats), false};  // a rest with no window at its start (ForceRest)
  }
  if (LogLine()) Log("prepare: 0x%08X rested - prepared spells can change in the rest's window", id);
}

// A creature that isn't a player character casts as in Part 1: every spell of its class at the levels it has slots
// for, where its list for that level is empty (a Cleric's, Druid's, Paladin's or Ranger's - NWN keeps no list - or
// a blueprint Wizard's, which has only memorised spells).
void FillCreatureLists(void* stats) {
  int added = 0;
  for (const CasterSlot& c : CasterSlots(stats)) {
    void* info = ClassInfoOf(stats, c.slot);
    const int top = TopLevel(c.row, c.level);
    for (int level = 0; level <= top; ++level) {
      if (level == 0 && rules::CantripsKnown2024(c.row, c.level) <= 0) continue;
      if (g_number_known(info, static_cast<uint8_t>(level)) != 0) continue;
      for (const SpellEntry& e : ClassSpells(c.row)) {
        if (e.level == level) g_add_known(info, static_cast<uint8_t>(level), static_cast<uint32_t>(e.id)), ++added;
      }
    }
  }
  if (added && LogLine()) {
    Log("prepare: 0x%08X (not a player character) knows every spell of its class it has slots for: %d added",
        ObjectIdOf(stats), added);
  }
}

// A player character's Wizard: its NWN spellbook (the known list's level 1+ spells) into a Spellbook item, once.
void MoveWizardBook(void* creature, void* stats) {
  for (const CasterSlot& c : CasterSlots(stats)) {
    if (c.row != kNwnClassWizard) continue;
    if (FindBook(creature)) return;
    std::vector<int> book = KnownFrom(stats, c.slot, 1, 9);
    void* info = ClassInfoOf(stats, c.slot);
    for (int id : book) {
      const SpellEntry* e = EntryOf(c.row, id);
      if (e) g_remove_known(info, static_cast<uint8_t>(e->level), static_cast<uint32_t>(id));
    }
    WriteBook(creature, book);
    if (LogLine()) Log("prepare: 0x%08X Wizard - its spellbook (%zu spells) moved to a Spellbook item", IdOf(creature),
                       book.size());
    return;
  }
}

bool NeedsPreparing(void* stats) {
  for (const CasterSlot& c : CasterSlots(stats)) {
    const int cantrips = static_cast<int>(Known(stats, c.slot, 0).size());
    const int prepared = static_cast<int>(KnownFrom(stats, c.slot, 1, 9).size());
    if (prepared == 0 && rules::PreparedSpells2024(c.row, c.level) > 0) return true;
    if (cantrips > rules::CantripsKnown2024(c.row, c.level) || prepared > rules::PreparedSpells2024(c.row, c.level)) {
      return true;
    }
  }
  return false;
}

// A list with room (or over its count): a class's cantrips or prepared spells, a Wizard's spellbook.
bool HasEmptyPlaces(void* creature, void* stats) {
  for (const CasterSlot& c : CasterSlots(stats)) {
    const int cantrips = static_cast<int>(Known(stats, c.slot, 0).size());
    const int prepared = static_cast<int>(KnownFrom(stats, c.slot, 1, 9).size());
    if (cantrips != rules::CantripsKnown2024(c.row, c.level)) return true;
    if (prepared != rules::PreparedSpells2024(c.row, c.level)) return true;
    if (c.row == kNwnClassWizard &&
        static_cast<int>(Book(creature).size()) < rules::SpellbookSpells2024(c.level)) {
      return true;
    }
  }
  return false;
}

std::map<uint32_t, int> g_levels;  // a player character's caster levels when last seen

int CasterLevels(void* stats) {
  int total = 0;
  for (const CasterSlot& c : CasterSlots(stats)) total += c.level * 16 + c.row;
  return total;
}

// The radial's Prepare Spells (with Turn Undead's class abilities; rules_quickbar.cpp also puts it on an empty
// quickbar slot, 0.75.3).
void GrantPrepareSpells(void* creature, void* stats) {
  if (engine::HasFeat(stats, rules::kFeatPrepareSpells)) return;
  const bool added = engine::AddFeat(stats, rules::kFeatPrepareSpells);
  if (LogLine()) Log("prepare: 0x%08X Prepare Spells (feat %d) %s", IdOf(creature), rules::kFeatPrepareSpells,
                    added ? "granted" : "REFUSED by the engine");
}

// A creature's lists looked at once (a local on it, so a loaded game's creatures are looked at again unless they were
// before it was saved): one that isn't a player character knows its class's spells; a player character gets the
// radial's Prepare Spells, a Wizard its Spellbook item, and the window when a list is empty or over its count.
void FirstSight(void* creature, void* stats) {
  engine::SetLocalInt(creature, kSeenVar, 1);
  if (!IsPlayer(creature)) {
    FillCreatureLists(stats);
    return;
  }
  if (CasterSlots(stats).empty()) return;
  GrantPrepareSpells(creature, stats);
  MoveWizardBook(creature, stats);
  if (NeedsPreparing(stats)) g_open_pending[IdOf(creature)] = {-1, false};
}

void Update(void* creature) {
  void* stats = StatsOf(creature);
  if (!stats) return;
  if (engine::GetLocalInt(creature, kSeenVar) == 0) FirstSight(creature, stats);
  if (!IsPlayer(creature)) return;
  const uint32_t id = IdOf(creature);
  // A level-up: the new places are to fill.
  const int levels = CasterLevels(stats);
  auto known_levels = g_levels.find(id);
  if (known_levels == g_levels.end()) {
    g_levels[id] = levels;
  } else if (known_levels->second != levels) {
    known_levels->second = levels;
    if (!CasterSlots(stats).empty()) {
      GrantPrepareSpells(creature, stats);
      MoveWizardBook(creature, stats);
      // 3b-1: NWN's Spells page fills the new places of a level-up; the window opens for what's left (the Wizard's
      // spellbook until 3b-2, a page closed short).
      if (!g_sessions.count(id) && !g_open_pending.count(id) && HasEmptyPlaces(creature, stats)) {
        g_open_pending[id] = {-1, false};
      }
    }
  }
  auto pending = g_open_pending.find(id);
  if (pending != g_open_pending.end()) {
    const PendingOpen open = pending->second;
    g_open_pending.erase(pending);
    const bool resting = (Field<uint32_t>(creature, ee::kCreatureActivities) & ee::kActivityResting) != 0;
    if (!open.resting || resting) Open(id, open.slot, open.resting);
    return;
  }
  auto found = g_sessions.find(id);
  if (found != g_sessions.end() && Waiting(found->second) &&
      !(Field<uint32_t>(creature, ee::kCreatureActivities) & ee::kActivityResting)) {
    g_sessions.erase(found);
    NuiClose(id, kWindow);
    TellPlayer(id, "nwn5e: the rest was interrupted - your spells stay as they were.");
    if (LogLine()) Log("prepare: 0x%08X the rest was interrupted - the window closed, no change", id);
  }
}

// The creatures that aren't player characters get their lists after a rest too (a henchman's level-up).
void RestDoneForCreatures(void* stats) {
  void* creature = CreatureOf(stats);
  if (creature && !IsPlayer(creature)) FillCreatureLists(stats);
}

// A scroll a Wizard learns: into its book, not its prepared spells.
int LearnScroll(void* creature, uint32_t item) {
  void* stats = StatsOf(creature);
  if (!stats || !IsPlayer(creature)) return g_learn_scroll(creature, item);
  int slot = -1;
  for (const CasterSlot& c : CasterSlots(stats)) {
    if (c.row == kNwnClassWizard) slot = c.slot;
  }
  if (slot < 0) return g_learn_scroll(creature, item);
  // 0.75.5 (owner: "PR8. grease is already in the spellbook"): a spell already in the book - the engine checks only
  // the known (prepared) list and would use the scroll up - is refused, the scroll kept.
  char code[700];
  sprintf_s(code,
            "object i = StringToObject(\"%x\"); int s = -1; itemproperty p = GetFirstItemProperty(i);\n"
            "while (GetIsItemPropertyValid(p)) { if (GetItemPropertyType(p) == ITEM_PROPERTY_CAST_SPELL) { s = "
            "StringToInt(Get2DAString(\"iprp_spells\", \"SpellIndex\", GetItemPropertySubType(p))); break; } p = "
            "GetNextItemProperty(i); }\nSetLocalInt(OBJECT_SELF, \"nwn5e_scroll_spell\", s);",
            item);
  engine::SetLocalInt(creature, "nwn5e_scroll_spell", -1);  // not an earlier scroll's, should the script not run
  RunScriptChunk(code, IdOf(creature));
  const int scroll_spell = engine::GetLocalInt(creature, "nwn5e_scroll_spell");
  if (scroll_spell >= 0 && Contains(Book(creature), scroll_spell)) {
    const SpellEntry* e = EntryOf(kNwnClassWizard, scroll_spell);
    TellPlayer(IdOf(creature), "nwn5e: " + (e ? e->name : std::string("that spell")) +
                                   " is already in your Spellbook - the scroll is kept.");
    if (LogLine()) Log("prepare: 0x%08X scroll of spell %d refused - already in the Spellbook", IdOf(creature), scroll_spell);
    return 0;
  }
  const std::vector<int> before = KnownFrom(stats, slot, 1, 9);
  const int result = g_learn_scroll(creature, item);
  std::vector<int> book = Book(creature);
  bool changed = false;
  for (int id : KnownFrom(stats, slot, 1, 9)) {
    if (Contains(before, id)) continue;
    const SpellEntry* e = EntryOf(kNwnClassWizard, id);
    if (e) g_remove_known(ClassInfoOf(stats, slot), static_cast<uint8_t>(e->level), static_cast<uint32_t>(id));
    if (!Contains(book, id)) book.push_back(id), changed = true;
    if (LogLine()) Log("prepare: 0x%08X learned spell %d from a scroll - into the Spellbook", IdOf(creature), id);
  }
  if (changed) WriteBook(creature, book);
  return result;
}

void OpenScript(uint32_t self) {
  void* creature = CreatureById(self);
  if (!IsPlayer(creature)) return;
  if (g_sessions.count(self)) return;
  Open(self, -1, false);
}

std::vector<std::string> PrepareCommand(void* creature, const std::string&) {
  OpenScript(IdOf(creature));
  return {};
}

}  // namespace

void PrepareSpellsNoteCast(void* creature) {
  if (!IsPlayer(creature) || !ChangeOpen(creature)) return;
  SetChangeOpen(creature, false);
  if (LogLine()) Log("prepare: 0x%08X cast a spell - the Long Rest's change is closed", IdOf(creature));
}

std::string PrepareSpellsReset(void* creature) {
  void* stats = StatsOf(creature);
  if (g_config.spellcasting != rules::RuleVariant::kFaithful || !g_number_known || !stats || !IsPlayer(creature)) {
    return {};
  }
  const uint32_t id = IdOf(creature);
  int removed = 0;
  bool wizard = false;
  for (const CasterSlot& c : CasterSlots(stats)) {
    wizard = wizard || c.row == kNwnClassWizard;
    void* info = ClassInfoOf(stats, c.slot);
    for (int level = 0; level <= 9; ++level) {
      for (int spell : Known(stats, c.slot, level)) {
        g_remove_known(info, static_cast<uint8_t>(level), static_cast<uint32_t>(spell));
        ++removed;
      }
    }
  }
  std::string code = "object b = GetItemPossessedBy(OBJECT_SELF, \"" + std::string(kBookTag) + "\");\n";
  code += wizard ? "" : "if (GetIsObjectValid(b)) { SetPlotFlag(b, FALSE); DestroyObject(b); }\n";
  code += "SetLocalString(OBJECT_SELF, \"" + std::string(kBookCopy) + "\", \"\");\n";
  RunScriptChunk(code.c_str(), id);
  if (wizard) WriteBook(creature, {});
  SetChangeOpen(creature, false);
  g_sessions.erase(id);
  if (CasterSlots(stats).empty()) return {};
  g_open_pending[id] = {-1, false};
  if (LogLine()) Log("prepare: 0x%08X !test - lists emptied (%d spells), the window opens", id, removed);
  return "  spells: cantrips and prepared spells empty" + std::string(wizard ? ", an empty Spellbook" : "") +
         " - the Prepare spells window opens";
}

void InstallPrepareSpells() {
  if (g_config.spellcasting != rules::RuleVariant::kFaithful) return;
  g_add_known = reinterpret_cast<AddKnownFn>(EngineExport(ee::kClassInfoAddKnownSpell));
  g_remove_known = reinterpret_cast<RemoveKnownFn>(EngineExport(ee::kClassInfoRemoveKnownSpell));
  g_get_known = reinterpret_cast<GetKnownFn>(EngineExport(ee::kClassInfoGetKnownSpell));
  g_number_known = reinterpret_cast<NumberKnownFn>(EngineExport(ee::kClassInfoGetNumberKnownSpells));
  g_get_spell = reinterpret_cast<GetSpellFn>(EngineExport(ee::kGetSpell));
  g_spell_level = reinterpret_cast<SpellLevelFn>(EngineExport(ee::kSpellGetLevel));
  g_spell_name = reinterpret_cast<SpellNameFn>(EngineExport(ee::kSpellGetNameText));
  g_free_text = reinterpret_cast<ExoStringFreeFn>(EngineExport(ee::kExoStringDestructor));
  if (!g_add_known || !g_remove_known || !g_get_known || !g_number_known || !g_get_spell || !g_spell_level ||
      !g_spell_name || !g_free_text) {
    Log("SKIP prepare: an engine function for the known lists is missing - no Prepare spells window");
    return;
  }
  const bool window = AddNuiWindow(kWindow, &OnEvent);
  const bool update = AddCreatureUpdateListener(&Update);
  const bool rest = AddRestStartListener(&RestBegun) && AddRestListener(&RestDone) &&
                    AddRestListener(&RestDoneForCreatures);
  const bool scrolls = HookExport("CNWSCreature::LearnScroll (a Wizard's scroll into its Spellbook, #36)",
                                  ee::kCreatureLearnScroll, reinterpret_cast<void*>(&LearnScroll),
                                  reinterpret_cast<void**>(&g_learn_scroll));
  AddScriptHandler("n5_prepspl", &OpenScript);
  AddChatCommand("!prepare", &PrepareCommand);
  Log("prepare: the Prepare spells window (radial Prepare Spells, !prepare; opens as a rest begins, Accept at its end); "
      "a Wizard's Spellbook item; other creatures know their class's spells (#36 Part 3a)%s",
      window && update && rest && scrolls ? "" : " - SOME hooks missing");
}
