// nwn5e_rage - Rage in the 2024 rules (class_features faithful; docs/PLAN.md
// "Phase 3 - Barbarian", B1-B3). Spell 307's impact script in faithful (the
// served spells.2da row; NWN mode keeps nw_s1_barbrage).
//
// A marker only: an effect of the Rage spell, which the game's AI reads
// (GetHasFeatEffect(FEAT_BARBARIAN_RAGE): no casting, no raging again), and a
// local the DLL picks up. nwn5e's DLL does the rest (rules_barbarian.cpp):
// resistance, Rage Damage, advantage, Pounce, Mindless Rage, the icon, the end.
//
// Compiled with 1.69's clcompile (1.69 functions only: EE keeps their
// indices) into data/files/nwn5e_rage.ncs.
void main()
{
    PlayVoiceChat(VOICE_CHAT_BATTLECRY1);
    SignalEvent(OBJECT_SELF, EventSpellCastAt(OBJECT_SELF, SPELLABILITY_BARBARIAN_RAGE, FALSE));
    effect eRage = ExtraordinaryEffect(EffectVisualEffect(VFX_DUR_CESSATE_POSITIVE));
    ApplyEffectToObject(DURATION_TYPE_TEMPORARY, eRage, OBJECT_SELF, 600.0);
    ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_IMPROVE_ABILITY_SCORE), OBJECT_SELF);
    SetLocalInt(OBJECT_SELF, "nwn5e_rage_start", 1);
}
