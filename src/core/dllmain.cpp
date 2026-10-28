// nwn5e.dll for NWN: Enhanced Edition (64-bit) — loaded into nwmain.exe by
// nwn5e.exe before the game's entry point. Engine functions are found by
// their exported names (docs/EE-RECON.md); what is ported so far and in what
// order: docs/EE-PORT.md.

#include <windows.h>
#include <shlwapi.h>

#include "config.h"
#include "hooks.h"
#include "log.h"

#pragma comment(lib, "shlwapi.lib")

void InstallCritRules();
void InstallAttackRules();
void InstallExtraAttackRules();
void InstallSheetRules();
void InstallDebugSwitches();
void InstallRacialRules();
void InstallTwoWeaponRules();
void InstallAdvantageRules();
void InstallRetargetRules();
void InstallOpportunityRules();
void InstallPotionRules();
void InstallConditionRules();
void InstallMasteryRules();
void InstallSaveRules();
void InstallSaveRollRules();
void InstallLabelRules();
void InstallSkillRules();
void InstallNwnOnlySkillRules();
void InstallCastingRules();
void InstallInitiativeRules();
void InstallSpellAttackRules();
void InstallShoveRules();
void InstallGrappleRules();
void InstallRogueRules();
void InstallCombatOptions();
void InstallThiefRules();
void InstallSkillDcRules();
void InstallPerceptionRules();
void InstallDataServing(const wchar_t* dll_directory);
void InstallProgressionRules();
void InstallAlignmentRules();
void InstallHitPointRules();
void InstallFeatCadenceRules();
void InstallAbilityRules();
void InstallFeatureRules();
void InstallFighterRules();
void InstallBarbarianRules();
void InstallMonkRules();
void InstallBarbarianStrikes();
void InstallRestRules();
void InstallDyingRules();
void InstallShortRestButton();
void InstallVfxBrowser();
void InstallAnimBrowser();
void InstallVfxEditor();
void InstallAnimEditor();
void InstallLowHpLook();
void InstallStanceLook();
void InstallStealthWatch();
void InstallLightProbe();
void InstallStealthGhost();
void InstallLightSources();
void InstallAnimTrace();
void InstallDriveFix();
void InstallAnimLayers();
void InstallRadialClassFeats();
void InstallRadialSpecialAttacks();
void InstallEquipCancelFix();
void InstallQuickbar();
void InstallNuiTest();
void InstallClientUi();
void InstallCreationPages();
void InstallCreationSteps();
void InstallFeatRules();
void InstallSkillsPage();
void InstallStartingEquipment();
void InstallCarrying();
void InstallArmorRules();
void InstallWeaponSizeRules();
void InstallFinesseRules();
void InstallTestCommands();
void InstallOriginFeats();
#ifdef NWN5E_PRIVATE
void InstallPrivateRules();  // private/src (docs/PRIVATE-LAYER.md)
#endif

namespace {

constexpr const char* kVersion = "0.52.1";

void Start(HMODULE self) {
  wchar_t directory[MAX_PATH];
  GetModuleFileNameW(self, directory, MAX_PATH);
  PathRemoveFileSpecW(directory);
  LogOpen(directory);
  Log("nwn5e-ee %s loaded", kVersion);
#ifdef NWN5E_PRIVATE
  Log("private layer: compiled in (personal use only; never share this DLL)");
#endif
  wchar_t host[MAX_PATH];
  GetModuleFileNameW(nullptr, host, MAX_PATH);
  Log("host: %ls", host);
  ConfigLoad(directory);
  if (!HooksBegin()) return;
  InstallDataServing(directory);  // first: its strings feed the label hook
  InstallCritRules();
  InstallAttackRules();
  InstallExtraAttackRules();
  InstallSheetRules();
  InstallRacialRules();
  InstallTwoWeaponRules();
  InstallAdvantageRules();
  InstallRetargetRules();
  InstallOpportunityRules();
  InstallPotionRules();
  InstallConditionRules();
  InstallMasteryRules();
  InstallSaveRules();
  InstallSaveRollRules();
  InstallLabelRules();
  InstallSkillRules();
  InstallNwnOnlySkillRules();
  InstallCastingRules();
  InstallInitiativeRules();
  InstallSpellAttackRules();
  InstallShoveRules();
  InstallGrappleRules();
  InstallRogueRules();
  InstallCombatOptions();
  InstallThiefRules();
  InstallSkillDcRules();
  InstallPerceptionRules();
  InstallProgressionRules();
  InstallAlignmentRules();
  InstallHitPointRules();
  InstallFeatCadenceRules();
  InstallAbilityRules();
  InstallFeatureRules();
  InstallFighterRules();
  InstallBarbarianRules();
  InstallBarbarianStrikes();
  InstallMonkRules();
  InstallRestRules();
  InstallDyingRules();
  InstallLowHpLook();
  InstallStanceLook();
  InstallStealthWatch();
  InstallLightProbe();
  InstallStealthGhost();
  InstallLightSources();
  InstallAnimLayers();
  InstallRadialClassFeats();
  InstallRadialSpecialAttacks();
  InstallEquipCancelFix();
  InstallQuickbar();
  InstallAnimTrace();
  InstallDriveFix();
  InstallShortRestButton();
  InstallVfxBrowser();
  InstallAnimBrowser();
  InstallVfxEditor();
  InstallAnimEditor();
  InstallNuiTest();
  InstallClientUi();
  InstallCreationPages();
  InstallCreationSteps();
  InstallFeatRules();
  InstallSkillsPage();
  InstallStartingEquipment();
  InstallCarrying();
  InstallArmorRules();
  InstallWeaponSizeRules();
  InstallFinesseRules();
  InstallTestCommands();
  InstallOriginFeats();
  InstallDebugSwitches();
#ifdef NWN5E_PRIVATE
  InstallPrivateRules();
#endif
  Log(HooksCommit() ? "hooks active" : "hooks NOT active");
}

}  // namespace

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(module);
    Start(module);
  }
  return TRUE;
}
