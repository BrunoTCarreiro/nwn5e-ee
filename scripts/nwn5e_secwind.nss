//::///////////////////////////////////////////////
//:: nwn5e_secwind - Fighter: Second Wind (SRD 5.2)
//:://////////////////////////////////////////////
/*
    Regain 1d10 + your Fighter level hit points (PLAN.md "Phase 3 -
    Fighter", F2). The uses (2, 3 at Fighter 4, 4 at 10) are the feat's uses
    per day, restored on rest. Prestige-class levels count as Fighter levels
    (decision 10): Arcane Archer 29, Weapon Master 33, Dwarven Defender 36,
    Purple Dragon Knight 41.
    From Fighter 5, Tactical Shift (F7): for the rest of the round, moving
    provokes no opportunity attacks (nwn5e's rule reads nwn5e_tactical_shift).
*/
void main()
{
    object oSelf = OBJECT_SELF;
    int nFighter = GetLevelByClass(CLASS_TYPE_FIGHTER, oSelf)
                 + GetLevelByClass(29, oSelf)
                 + GetLevelByClass(33, oSelf)
                 + GetLevelByClass(36, oSelf)
                 + GetLevelByClass(41, oSelf);
    int nHeal = d10() + nFighter;
    ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectHeal(nHeal), oSelf);
    ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HEALING_S), oSelf);
    if (nFighter >= 5)
    {
        SetLocalInt(oSelf, "nwn5e_tactical_shift", 1);
        DelayCommand(6.0, DeleteLocalInt(oSelf, "nwn5e_tactical_shift"));
    }
}
