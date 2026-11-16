// #39 Part 2a, build 4d (owner, 2026-11-16: "for the floating disk, can it work like a henchman? They have their own
// inventory, right?" - "go with the henchman disk"): Floating Disk (SRD 5.2.1; rules/floating_disk.h).
// - The disk is a henchman of its caster's: NWN's OpenInventory opens a player's henchman's inventory and no other
//   associate's (ExecuteCommandOpenInventory: the master and associate type 1). It's made from NWN's summoned Lantern
//   Archon (no items, no conversation) and given the animation session's disk (0.99.56: appearance 14000-14003) -
//   plot, Ethereal (enemies don't see it), Strength 30 (its load never slows it), its event scripts its own.
// - It takes no henchman's place: CNWSCreature::AddAssociate refuses a henchman past the module's limit (feedback
//   0x31), so the limit is the module's own plus one for each disk out (module locals n5d_base, n5d_disks; put back
//   by its heartbeat after a load).
// - Clicked (a conversation with no line matched) or the associate radial's Inventory: its inventory opens for its
//   caster. Stand your ground: it stays; Follow me: it follows again; Leave the party: the spell ends.
// - Every second (and its heartbeat, and its inventory's disturbance): it waits within 20 feet of its caster and
//   follows farther; its hour up (its mark), more than 500 pounds on it (GetWeight), more than 100 feet off (or another
//   area), or its caster gone - the spell ends: everything on it falls where it floats.
// - 0.99.55 (owner, 2026-11-16: "4 stages to it ... the mesh that was on top is placed where the disk was and works
//   as a container"; "go with all your recommendations"): its look shows its load's stage (rules::DiskStageOf -
//   swapped as it changes), and its load falls into that stage's pile, a container where it floated, gone once
//   closed empty (n5_d_pile). 0.99.56: the animation session's art - the disk's four appearance rows (14000 + its
//   stage), its piles nwn5e's blueprints n5_fd_pile1-3 on their placeables rows (0.99.55's treasure-pile placeholders
//   ran NWN's random-treasure scripts on opening).
// - A new casting with a disk out renews its hour.
// - 0.99.57 (owner, 2026-11-16, FD-9: "loading a save makes the floating shield uninteractible and stuck in place"):
//   the save had the disk intact (its scripts, locals and load, told to stand its ground in FD-8) but after the load
//   no handler of it logged anything - clicks unanswered, !time 2 not ending its hour. Its handlers now log their
//   first calls after a game starts (and why one stops), and its caster looks after it too: every second a player
//   character's disk (its local n5d_disk) is checked - its end, its stage, its following - whatever its own scripts do.
// - Kill XP goes to a party's player characters alone (CNWSCreature::ApplyDeathExperience's IsPlayerCharacter check):
//   the disk takes none.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <unordered_map>

#include "chat.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "script_hooks.h"
#include "spell_impact.h"
#include "rules/floating_disk.h"
#include "rules/spells2024.h"

void FloatingDiskSpell(uint32_t self, void* caster, int row);  // rules_spells2024.cpp (UtilitySpell)
bool InstallFloatingDisk();                                    // rules_spells2024.cpp's install

namespace {

constexpr float kMetresPerFoot = 0.3048f;
constexpr ULONGLONG kCheckEveryMs = 1000;
constexpr const char* kDiskBlueprint = "nw_s_clantern";  // NWN's summoned Lantern Archon: no items, no conversation

std::unordered_map<uint32_t, ULONGLONG> g_disks;  // a disk out -> its next check
std::unordered_map<uint32_t, ULONGLONG> g_caster_look;  // a player character -> its next look for its disk
int g_calls_logged = 0;  // since the game started: the handlers' first calls (0.99.57)

int g_logged = 0;
bool LogSlot() { return g_logged < 300 && ++g_logged; }

void Run(const std::string& code, uint32_t self, const char* what) {
  const int result = RunScriptChunk(code.c_str(), self);
  if (result != 0) Log("floating disk: %s - its script returned %d", what, result);
}

uint32_t ParseHex(const std::string& text) { return static_cast<uint32_t>(strtoul(text.c_str(), nullptr, 16)); }

void Replace(std::string* text, const std::string& key, const std::string& value) {
  for (size_t at = text->find(key); at != std::string::npos; at = text->find(key, at + value.size())) {
    text->replace(at, key.size(), value);
  }
}

using SetBaseFn = void (*)(void* stats, uint8_t value);
void SetStrength(void* creature, int value) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  const uintptr_t setter = EngineExport(ee::kSetAbilityBase[0]);
  if (stats && setter) reinterpret_cast<SetBaseFn>(setter)(stats, static_cast<uint8_t>(value));
}

// The cast, run as the caster: a new disk (n5d_new), or the one out's hour renewed (n5d_renewed).
const char* const kCast = R"nss(
object m = OBJECT_SELF;
object d = GetLocalObject(m, "n5d_disk");
SetLocalString(m, "n5d_new", ""); SetLocalString(m, "n5d_renewed", "");
if (GetIsObjectValid(d) && GetLocalInt(d, "n5d_disk") && GetMaster(d) == m) {
  effect e = GetFirstEffect(d);
  while (GetIsEffectValid(e)) { if (GetEffectTag(e) == "n5s_disk") RemoveEffect(d, e); e = GetNextEffect(d); }
  ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(SupernaturalEffect(EffectCutsceneGhost()), $ROW), "n5s_disk"), d, HoursToSeconds($HOURS));
  SetLocalString(m, "n5d_renewed", ObjectToString(d));
} else {
  d = CreateObject(OBJECT_TYPE_CREATURE, "$BLUEPRINT", GetLocation(m), FALSE, "n5s_floatdisk");
  if (GetIsObjectValid(d)) {
    SetCreatureAppearanceType(d, $APP);
    SetObjectVisualTransform(d, OBJECT_VISUAL_TRANSFORM_SCALE, $SCALE);
    SetLocalInt(d, "n5d_stage", 0);
    SetName(d, "Floating Disk");
    int ev;
    for (ev = 5000; ev <= 5012; ev++) SetEventScript(d, ev, "");
    SetEventScript(d, EVENT_SCRIPT_CREATURE_ON_HEARTBEAT, "n5_d_hb");
    SetEventScript(d, EVENT_SCRIPT_CREATURE_ON_DISTURBED, "n5_d_dist");
    SetEventScript(d, EVENT_SCRIPT_CREATURE_ON_DIALOGUE, "n5_d_talk");
    SetListening(d, TRUE);
    ApplyEffectToObject(DURATION_TYPE_PERMANENT, SupernaturalEffect(EffectEthereal()), d);
    ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(SupernaturalEffect(EffectCutsceneGhost()), $ROW), "n5s_disk"), d, HoursToSeconds($HOURS));
    SetPlotFlag(d, TRUE);
    SetLocalInt(d, "n5d_disk", 1);
    SetLocalObject(d, "n5d_master", m);
    object mod = GetModule();
    if (GetLocalInt(mod, "n5d_disks") <= 0) SetLocalInt(mod, "n5d_base", GetMaxHenchmen());
    SetLocalInt(mod, "n5d_disks", GetLocalInt(mod, "n5d_disks") + 1);
    SetMaxHenchmen(GetLocalInt(mod, "n5d_base") + GetLocalInt(mod, "n5d_disks"));
    AddHenchman(m, d);
    SetLocalObject(m, "n5d_disk", d);
    SetLocalString(m, "n5d_new", ObjectToString(d));
    SetLocalInt(m, "n5d_limit", GetMaxHenchmen());
    SetLocalInt(m, "n5d_joined", GetMaster(d) == m);
    ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_FNF_SUMMON_MONSTER_1), d);
  }
}
)nss";

// Its state, run as the disk; the module's henchman limit put back (a loaded game).
const char* const kState = R"nss(
object d = OBJECT_SELF; object m = GetLocalObject(d, "n5d_master");
int here = GetIsObjectValid(m) && GetMaster(d) == m;
SetLocalInt(d, "n5d_here", here);
SetLocalInt(d, "n5d_same", here && GetArea(m) == GetArea(d));
SetLocalInt(d, "n5d_ft10", here ? FloatToInt(GetDistanceBetween(d, m) / 0.3048 * 10.0) : 0);
SetLocalInt(d, "n5d_wt", GetWeight(d));
SetLocalInt(d, "n5d_mark", GetHasSpellEffect($ROW, d));
SetLocalString(d, "n5d_mid", here ? ObjectToString(m) : "");
object mod = GetModule();
int want = GetLocalInt(mod, "n5d_base") + GetLocalInt(mod, "n5d_disks");
if (GetLocalInt(mod, "n5d_disks") > 0 && GetMaxHenchmen() < want) SetMaxHenchmen(want);
)nss";

// The end, run as the disk: its load falls where it floats (gold back to its caster), it leaves the party, the
// module's henchman limit comes down, and it goes.
const char* const kEnd = R"nss(
object d = OBJECT_SELF; object m = GetLocalObject(d, "n5d_master"); location l = GetLocation(d);
int n = 0; object i;
object pile = OBJECT_INVALID;
if ("$PILE" != "") {
  pile = CreateObject(OBJECT_TYPE_PLACEABLE, "$PILE", l, FALSE, "n5s_diskpile");
  if (GetIsObjectValid(pile)) {
    SetName(pile, "Floating Disk's load");
    SetEventScript(pile, EVENT_SCRIPT_PLACEABLE_ON_CLOSED, "n5_d_pile");
    SetLocalInt(pile, "n5d_pile", 1);
  }
}
i = GetFirstItemInInventory(d);
while (GetIsObjectValid(i)) { CopyObject(i, l, pile, "", TRUE); SetPlotFlag(i, FALSE); DestroyObject(i); n++; i = GetNextItemInInventory(d); }
int s;
for (s = 0; s < 14; s++) { i = GetItemInSlot(s, d); if (GetIsObjectValid(i)) { CopyObject(i, l, pile, "", TRUE); SetPlotFlag(i, FALSE); DestroyObject(i); n++; } }
SetLocalInt(d, "n5d_piled", GetIsObjectValid(pile));
if (GetIsObjectValid(pile) && n == 0) DestroyObject(pile);
if (GetGold(d) > 0 && GetIsObjectValid(m)) GiveGoldToCreature(m, GetGold(d));
SetLocalInt(d, "n5d_dropped", n);
if (GetIsObjectValid(m)) { if (GetMaster(d) == m) RemoveHenchman(m, d); DeleteLocalObject(m, "n5d_disk"); DeleteLocalInt(m, "n5d_has"); }
object mod = GetModule();
int k = GetLocalInt(mod, "n5d_disks") - 1;
if (k < 0) k = 0;
SetMaxHenchmen(GetLocalInt(mod, "n5d_base") + k);
SetLocalInt(mod, "n5d_disks", k);
if (k == 0) DeleteLocalInt(mod, "n5d_base");
SetLocalInt(d, "n5d_disk", 0);
ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_UNSUMMON), l);
SetPlotFlag(d, FALSE); SetIsDestroyable(TRUE, FALSE, FALSE); DestroyObject(d, 0.2);
)nss";

std::string Script(const char* code) {
  std::string text = code;
  Replace(&text, "$ROW", std::to_string(rules::kFloatingDisk));
  Replace(&text, "$HOURS", std::to_string(rules::kFloatingDiskHours));
  Replace(&text, "$BLUEPRINT", kDiskBlueprint);
  const rules::DiskLook empty = rules::DiskStageLook(rules::DiskStage::kEmpty);
  Replace(&text, "$APP", std::to_string(empty.appearance));
  char scale[16];
  sprintf_s(scale, "%.2f", empty.scale);
  Replace(&text, "$SCALE", scale);
  return text;
}

void EndDisk(uint32_t disk, rules::DiskEnd why) {
  void* creature = CreatureById(disk);
  g_disks.erase(disk);
  if (!creature || engine::GetLocalInt(creature, "n5d_disk") != 1) return;
  Run(Script(kState), disk, "the disk's last look");  // its load now (Leave the party comes between looks)
  uint32_t master = ParseHex(engine::GetLocalString(creature, "n5d_mid"));
  if (!master && why == rules::DiskEnd::kSentAway) master = ParseHex(engine::GetLocalString(creature, "n5d_talker"));
  const int weight = engine::GetLocalInt(creature, "n5d_wt");
  const rules::DiskStage stage = rules::DiskStageOf(weight);
  const char* pile = rules::DiskPileBlueprint(stage);
  std::string code = Script(kEnd);
  Replace(&code, "$PILE", pile ? pile : "");
  Run(code, disk, "the disk's end");
  const int dropped = engine::GetLocalInt(creature, "n5d_dropped");
  const bool piled = engine::GetLocalInt(creature, "n5d_piled") != 0;
  if (master) {
    std::string words = std::string("nwn5e: Floating Disk ends - ") + rules::DiskEndText(why);
    if (dropped > 0) {
      words += "; everything on it (" + std::to_string(dropped) + (dropped == 1 ? " item" : " items") +
               (piled ? ") falls in a pile where it floated - open it to take them back." : ") falls to the ground.");
    } else {
      words += ".";
    }
    TellPlayer(master, words);
  }
  if (LogSlot()) {
    Log("floating disk: 0x%08X (caster 0x%08X) ends - %s; %d items (%.1f pounds, stage %d) %s", disk, master,
        rules::DiskEndText(why), dropped, weight / 10.0, static_cast<int>(stage),
        piled ? (std::string("in a pile (") + pile + ")").c_str() : "on the ground");
  }
}

// Its look at its caster: the spell ends, or it follows (unless told to stand its ground).
void CheckDisk(uint32_t disk, const char* by) {
  void* creature = CreatureById(disk);
  if (!creature || engine::GetLocalInt(creature, "n5d_disk") != 1) {
    if (g_calls_logged < 40 && ++g_calls_logged) {
      Log("floating disk: 0x%08X (%s) not looked at - %s", disk, by,
          !creature ? "no creature by that id" : "its n5d_disk local isn't 1");
    }
    g_disks.erase(disk);
    return;
  }
  if (g_calls_logged < 40 && ++g_calls_logged) {
    Log("floating disk: 0x%08X looked at (%s) - stay %d, stage %d, load %d tenths of a pound", disk, by,
        engine::GetLocalInt(creature, "n5d_stay"), engine::GetLocalInt(creature, "n5d_stage"),
        engine::GetLocalInt(creature, "n5d_wt"));
  }
  g_disks[disk] = GetTickCount64() + kCheckEveryMs;
  Run(Script(kState), disk, "the disk's look");
  const float feet = static_cast<float>(engine::GetLocalInt(creature, "n5d_ft10")) / 10.0f;
  const rules::DiskEnd why = rules::DiskEndReason(
      engine::GetLocalInt(creature, "n5d_here") != 0, engine::GetLocalInt(creature, "n5d_same") != 0, feet,
      engine::GetLocalInt(creature, "n5d_wt"), engine::GetLocalInt(creature, "n5d_mark") != 0);
  if (why != rules::DiskEnd::kNone) {
    // Its caster gone, its words go nowhere; the master's id comes from the look (kState) - "" then.
    EndDisk(disk, why);
    return;
  }
  const int weight = engine::GetLocalInt(creature, "n5d_wt");
  const rules::DiskStage stage = rules::DiskStageOf(weight);
  if (static_cast<int>(stage) != engine::GetLocalInt(creature, "n5d_stage")) {
    const rules::DiskLook look = rules::DiskStageLook(stage);
    char code[220];
    sprintf_s(code,
              "SetCreatureAppearanceType(OBJECT_SELF, %d); SetObjectVisualTransform(OBJECT_SELF, "
              "OBJECT_VISUAL_TRANSFORM_SCALE, %.2f);",
              look.appearance, look.scale);
    Run(code, disk, "the disk's stage");
    if (LogSlot()) {
      Log("floating disk: 0x%08X its load %.1f pounds - stage %d (appearance %d at %.2f)", disk, weight / 10.0,
          static_cast<int>(stage), look.appearance, look.scale);
    }
    engine::SetLocalInt(creature, "n5d_stage", static_cast<int>(stage));
  }
  if (rules::DiskFollows(feet, engine::GetLocalInt(creature, "n5d_stay") != 0)) {
    Run("if (GetCurrentAction() == ACTION_INVALID) ActionMoveToObject(GetLocalObject(OBJECT_SELF, \"n5d_master\"), TRUE, "
        "4.0);",
        disk, "the disk's follow");
  }
}

void DiskHeartbeat(uint32_t self) { CheckDisk(self, "its heartbeat"); }
void DiskDisturbed(uint32_t self) { CheckDisk(self, "its inventory"); }  // more than 500 pounds ends it at once

// Its caster's word: a click or Inventory opens it; Stand your ground, Follow me; Leave the party ends it.
void DiskTalk(uint32_t self) {
  void* creature = CreatureById(self);
  if (!creature || engine::GetLocalInt(creature, "n5d_disk") != 1) {
    if (g_calls_logged < 40 && ++g_calls_logged) {
      Log("floating disk: 0x%08X a word not heard - %s", self,
          !creature ? "no creature by that id" : "its n5d_disk local isn't 1");
    }
    return;
  }
  Run("SetLocalInt(OBJECT_SELF, \"n5d_pat\", GetListenPatternNumber()); SetLocalInt(OBJECT_SELF, \"n5d_bymaster\", "
      "GetLastSpeaker() == GetLocalObject(OBJECT_SELF, \"n5d_master\")); SetLocalString(OBJECT_SELF, \"n5d_mid\", "
      "ObjectToString(GetLocalObject(OBJECT_SELF, \"n5d_master\"))); SetLocalString(OBJECT_SELF, \"n5d_talker\", "
      "ObjectToString(GetLocalObject(OBJECT_SELF, \"n5d_master\")));",
      self, "the disk's listening");
  const int pattern = engine::GetLocalInt(creature, "n5d_pat");
  const rules::DiskOrder order = rules::DiskOrderOf(pattern);
  const uint32_t master = ParseHex(engine::GetLocalString(creature, "n5d_mid"));
  if (LogSlot()) {
    Log("floating disk: 0x%08X hears %d (%s)%s", self, pattern,
        order == rules::DiskOrder::kOpen     ? "open"
        : order == rules::DiskOrder::kStay   ? "stay"
        : order == rules::DiskOrder::kFollow ? "follow"
        : order == rules::DiskOrder::kEnd    ? "end"
                                             : "nothing it does",
        engine::GetLocalInt(creature, "n5d_bymaster") ? "" : " - not its caster's");
  }
  if (!engine::GetLocalInt(creature, "n5d_bymaster")) return;
  switch (order) {
    case rules::DiskOrder::kOpen:
      Run("OpenInventory(OBJECT_SELF, GetLocalObject(OBJECT_SELF, \"n5d_master\"));", self, "the disk's inventory");
      break;
    case rules::DiskOrder::kStay:
      engine::SetLocalInt(creature, "n5d_stay", 1);
      Run("ClearAllActions();", self, "the disk's stay");
      TellPlayer(master, "nwn5e: Floating Disk - it stays where it is (more than 100 feet from it, the spell ends).");
      break;
    case rules::DiskOrder::kFollow:
      engine::SetLocalInt(creature, "n5d_stay", 0);
      TellPlayer(master, "nwn5e: Floating Disk - it follows you again (within 20 feet).");
      break;
    case rules::DiskOrder::kEnd: EndDisk(self, rules::DiskEnd::kSentAway); break;
    default: break;
  }
}

// Every creature update: a disk out looks at its caster every second.
void DiskUpdate(void* creature) {
  if (!creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const ULONGLONG now = GetTickCount64();
  // 0.99.57: a player character with a disk out looks after it every second (its local n5d_disk), whatever the
  // disk's own scripts do - after a loaded game too.
  // Its first update after a game starts looks too: a disk from a save before 0.99.57 has no n5d_has on its caster.
  if (Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0 &&
      (engine::GetLocalInt(creature, "n5d_has") == 1 || !g_caster_look.count(id))) {
    ULONGLONG& next = g_caster_look[id];
    if (now >= next) {
      next = now + kCheckEveryMs;
      Run("object d = GetLocalObject(OBJECT_SELF, \"n5d_disk\"); SetLocalString(OBJECT_SELF, \"n5d_find\", "
          "GetIsObjectValid(d) ? ObjectToString(d) : \"\");",
          id, "the caster's look for its disk");
      const uint32_t disk = ParseHex(engine::GetLocalString(creature, "n5d_find"));
      if (!disk) {
        engine::SetLocalInt(creature, "n5d_has", 0);  // gone
      } else {
        engine::SetLocalInt(creature, "n5d_has", 1);
        if (!g_disks.count(disk) && g_calls_logged < 40 && ++g_calls_logged) {
          Log("floating disk: 0x%08X its caster 0x%08X finds it (a loaded game, or its own update missed)", disk, id);
        }
        CheckDisk(disk, "its caster");
      }
    }
  }
  if (g_disks.empty()) return;
  const auto it = g_disks.find(id);
  if (it == g_disks.end() || now < it->second) return;
  CheckDisk(id, "its update");
}

// The pile, closed: gone once empty.
void PileClosed(uint32_t self) {
  Run("if (GetLocalInt(OBJECT_SELF, \"n5d_pile\") && !GetIsObjectValid(GetFirstItemInInventory(OBJECT_SELF)) && "
      "GetGold(OBJECT_SELF) == 0) DestroyObject(OBJECT_SELF, 0.5);",
      self, "the disk's pile");
}

void DisksGameStarted() {  // a loaded game's disks: their caster (or heartbeat) finds them again
  if (!g_disks.empty()) Log("floating disk: a game started - %zu disks forgotten until found again", g_disks.size());
  g_disks.clear();
  g_caster_look.clear();
  g_calls_logged = 0;
}

}  // namespace

void FloatingDiskSpell(uint32_t self, void* caster, int) {
  Run(Script(kCast), self, "Floating Disk");
  const uint32_t renewed = ParseHex(engine::GetLocalString(caster, "n5d_renewed"));
  if (renewed) {
    TellPlayer(self, "nwn5e: Floating Disk - your disk's hour starts again.");
    if (LogSlot()) Log("floating disk: 0x%08X renews its disk 0x%08X", self, renewed);
    return;
  }
  const uint32_t disk = ParseHex(engine::GetLocalString(caster, "n5d_new"));
  void* creature = disk ? CreatureById(disk) : nullptr;
  if (!creature) {
    TellPlayer(self, "nwn5e: Floating Disk - no disk came (see nwn5e.log).");
    Log("floating disk: 0x%08X - CreateObject(%s) gave no creature", self, kDiskBlueprint);
    return;
  }
  const int limit = engine::GetLocalInt(caster, "n5d_limit");
  if (!engine::GetLocalInt(caster, "n5d_joined")) {
    engine::SetLocalString(creature, "n5d_mid", "");
    Log("floating disk: 0x%08X - its disk 0x%08X didn't join the party (henchman limit %d)", self, disk, limit);
    EndDisk(disk, rules::DiskEnd::kMasterGone);
    TellPlayer(self, "nwn5e: Floating Disk - the disk couldn't join your party (see nwn5e.log).");
    return;
  }
  SetStrength(creature, rules::kFloatingDiskStrength);
  engine::SetLocalInt(caster, "n5d_has", 1);  // its caster looks after it (0.99.57)
  g_disks[disk] = GetTickCount64() + kCheckEveryMs;
  TellPlayer(self, "nwn5e: Floating Disk - a disk of force follows you for an hour: your henchman (it takes no "
                   "henchman's place). Click it, or Inventory on your associate radial, to load it - up to 500 pounds; "
                   "more and the spell ends, everything falling. It stays within 20 feet of you; more than 100 feet "
                   "from it, the spell ends. Leave the party (your radial) ends it. Its load shows on it, and falls in "
                   "a pile where it floated when the spell ends.");
  if (LogSlot()) {
    Log("floating disk: 0x%08X Floating Disk - 0x%08X, its henchman (the module's limit now %d); appearance %d at "
        "%.2f, Strength %d",
        self, disk, limit, rules::kFloatingDiskAppearance, rules::kFloatingDiskScale, rules::kFloatingDiskStrength);
  }
}

bool InstallFloatingDisk() {
  const bool ok = AddScriptHandler("n5_d_hb", &DiskHeartbeat) && AddScriptHandler("n5_d_dist", &DiskDisturbed) &&
                  AddScriptHandler("n5_d_talk", &DiskTalk) && AddScriptHandler("n5_d_pile", &PileClosed) &&
                  AddCreatureUpdateListener(&DiskUpdate) &&
                  AddGameStartListener(&DisksGameStarted);
  Log("floating disk: Floating Disk (#39 build 4d) - a henchman disk: its inventory up to 500 pounds, it follows past "
      "20 feet, ends past 100; its look its load's stage, its load a pile when it ends%s",
      ok ? "" : " - SOME hooks missing");
  return ok;
}
