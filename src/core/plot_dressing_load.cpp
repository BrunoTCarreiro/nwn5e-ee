// #172 (owner, 2026-11-19: "D′, with the switch on by default"; docs/PLAN.md, #39 Part 2, level 2's build 1a, "Which
// placeables break" and its revision): a module's Plot set dressing with nothing attached loads breakable. Called from
// light_sources.cpp's CNWSPlaceable::LoadPlaceable hook (the one load path for an area's placeables and a save's),
// after the engine has read it: the conversation and the event scripts come from the placeable's GFF struct
// (CResGFF::ReadFieldCResRef), the rest from the object. rules::PlotDressingBreaks decides; the hook clears the Plot
// flag. A save made after keeps it so.
#include <windows.h>

#include <stdint.h>

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "plot_dressing_load.h"
#include "rules/plot_dressing.h"

namespace {

// CResRef CResGFF::ReadFieldCResRef(CResStruct*, const char* field, int& success, const CResRef& default): a member
// returning a class, so the result's address comes after `this`.
using ReadResRefFn = void* (*)(void* gff, void* result, void* gff_struct, const char* field, int* success,
                               const void* fallback);
constexpr const char* kReadFieldCResRef =
    "?ReadFieldCResRef@CResGFF@@QEAA?AVCResRef@@PEAVCResStruct@@PEBDAEAHAEBV2@@Z";
ReadResRefFn g_read_resref = nullptr;
bool g_looked_up = false;

// A placeable's event script fields (a GIT's or a blueprint's), the trap-triggered one left out: it runs only when the
// placeable is trapped, and the toolset stamps its default (nw_t1_spikeminoc) on placeables that aren't.
const char* const kEventFields[] = {"OnClosed",        "OnDamaged",     "OnDeath",   "OnDisarm",  "OnHeartbeat",
                                    "OnInvDisturbed",  "OnLock",        "OnMeleeAttacked", "OnOpen", "OnSpellCastAt",
                                    "OnUnlock",        "OnUsed",        "OnUserDefined",   "OnClick"};

bool HasResRef(void* gff, void* gff_struct, const char* field) {
  char result[32] = {};
  char fallback[32] = {};
  int success = 0;
  g_read_resref(gff, result, gff_struct, field, &success, fallback);
  return success != 0 && result[0] != '\0';
}

int g_freed = 0;

}  // namespace

bool PlotDressingFreed(void* gff, void* gff_struct, rules::PlaceableFacts facts) {
  if (!g_config.plot_dressing_breakable || !facts.plot || !gff || !gff_struct) return false;
  if (!g_looked_up) {
    g_looked_up = true;
    g_read_resref = reinterpret_cast<ReadResRefFn>(EngineExport(kReadFieldCResRef));
    if (!g_read_resref) Log("SKIP plot dressing #172: CResGFF::ReadFieldCResRef not found - Plot dressing stays Plot");
  }
  if (!g_read_resref) return false;
  facts.has_conversation = HasResRef(gff, gff_struct, "Conversation");
  facts.scripts_besides_trap = 0;
  for (const char* field : kEventFields) {
    if (HasResRef(gff, gff_struct, field)) ++facts.scripts_besides_trap;
  }
  if (!rules::PlotDressingBreaks(g_config.plot_dressing_breakable,
                                 g_config.spellcasting == rules::RuleVariant::kFaithful, facts)) {
    return false;
  }
  if (++g_freed <= 5 || g_freed % 50 == 0) {
    Log("plot dressing: a Plot placeable with nothing attached loaded breakable (%d so far; house rule "
        "plot_dressing_breakable, #172)",
        g_freed);
  }
  return true;
}
