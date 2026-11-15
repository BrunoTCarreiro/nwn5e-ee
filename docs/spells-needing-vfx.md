# Spells that need VFX: the 2024 spells NWN doesn't have

A reference for making spell visuals: the SRD 5.2 spells (nwn5e's 2024 rules) with no counterpart in NWN:EE's own
spell list, so the game has no effects made for them. Compiled 2026-11-15 from the official SRD 5.2 and the game's
rows of nwn5e's served spells.2da (sources at the end). Spells only in the 2024 Player's Handbook aren't on it.

**How to read it**

- ✱ - **nwn5e already has the spell**, working in game with visuals borrowed from NWN's own effects
  (stand-ins). Its own VFX would be an upgrade. Each entry below names what it borrows today.
- No mark - **not in nwn5e yet**: the spell and its visuals both still to make.
- A box - tick it (`[x]`) when the spell has visuals of its own.
- A spell NWN has (even under another name, see the appendix) isn't listed, even where nwn5e hasn't reworked it yet.

**196 spells**: 34 in nwn5e with stand-ins (✱), 162 not in nwn5e yet; plus 23 to check by hand.

## Quick list

- **Cantrips (22):** Chill Touch✱, Dancing Lights✱, Druidcraft✱, Eldritch Blast, Elementalism✱, Fire Bolt✱, Guidance✱, Mage Hand✱, Mending, Message, Minor Illusion✱, Poison Spray✱, Prestidigitation✱, Produce Flame✱, Sacred Flame✱, Shillelagh✱, Shocking Grasp✱, Sorcerous Burst✱, Spare the Dying✱, Starry Wisp✱, Thaumaturgy✱, Vicious Mockery✱
- **Level 1 (31):** Alarm, Chromatic Orb✱, Command✱, Comprehend Languages, Create or Destroy Water, Detect Magic, Detect Poison and Disease, Disguise Self, Dissonant Whispers✱, Ensnaring Strike✱, Faerie Fire✱, False Life✱, Feather Fall✱, Floating Disk, Fog Cloud✱, Goodberry, Guiding Bolt✱, Healing Word✱, Hellish Rebuke, Heroism✱, Hex, Hunter's Mark✱, Illusory Script, Jump, Longstrider✱, Purify Food and Drink, Ray of Sickness✱, Searing Smite✱, Silent Image, Speak with Animals, Unseen Servant
- **Level 2 (31):** Animal Messenger, Arcane Lock, Arcanist's Magic Aura, Augury, Blur, Detect Thoughts, Dragon's Breath, Enlarge/Reduce, Enthrall, Flaming Sphere, Gentle Repose, Heat Metal, Levitate, Locate Animals or Plants, Locate Object, Magic Mouth, Mind Spike, Mirror Image, Misty Step, Moonbeam, Pass without Trace, Phantasmal Force, Protection from Poison, Rope Trick, Scorching Ray, Shining Smite, Spider Climb, Spiritual Weapon, Suggestion, Warding Bond, Zone of Truth
- **Level 3 (26):** Beacon of Hope, Blink, Conjure Animals, Counterspell, Create Food and Water, Daylight, Fly, Gaseous Form, Hypnotic Pattern, Major Image, Mass Healing Word, Meld into Stone, Nondetection, Phantom Steed, Plant Growth, Revivify, Sending, Sleet Storm, Speak with Dead, Speak with Plants, Spirit Guardians, Tiny Hut, Tongues, Water Breathing, Water Walk, Wind Wall
- **Level 4 (17):** Arcane Eye, Compulsion, Conjure Woodland Beings, Control Water, Dimension Door, Divination, Fabricate, Faithful Hound, Giant Insect, Guardian of Faith, Hallucinatory Terrain, Locate Creature, Private Sanctum, Resilient Sphere, Secret Chest, Stone Shape, Vitriolic Sphere
- **Level 5 (22):** Animate Objects, Antilife Shell, Commune, Commune with Nature, Contact Other Plane, Creation, Dream, Geas, Hallow, Mislead, Modify Memory, Passwall, Reincarnate, Scrying, Seeming, Summon Dragon, Telekinesis, Telepathic Bond, Teleportation Circle, Tree Stride, Wall of Force, Wall of Stone
- **Level 6 (20):** Conjure Fey, Contingency, Disintegrate, Eyebite, Find the Path, Forbiddance, Freezing Sphere, Guards and Wards, Heroes' Feast, Instant Summons, Irresistible Dance, Magic Jar, Mass Suggestion, Move Earth, Programmed Illusion, Transport via Plants, Wall of Ice, Wall of Thorns, Wind Walk, Word of Recall
- **Level 7 (10):** Forcecage, Magnificent Mansion, Mirage Arcane, Plane Shift, Project Image, Reverse Gravity, Sequester, Simulacrum, Symbol, Teleport
- **Level 8 (9):** Animal Shapes, Antipathy/Sympathy, Befuddlement, Clone, Control Weather, Demiplane, Glibness, Maze, Tsunami
- **Level 9 (8):** Astral Projection, Foresight, Imprisonment, Power Word Heal, Prismatic Wall, True Polymorph, True Resurrection, Wish

## Each spell

Name, school and casting classes; what it looks like in play (the visual it needs); nwn5e's status.

### Cantrips (22)

- [ ] **Chill Touch** ✱ - Necromancy; Sorcerer, Warlock, Wizard. Needs: touch, necrotic hand. nwn5e: in nwn5e (row 1051), borrows cast hand vco_mehanevil03; hit VFX_IMP_NEGATIVE_ENERGY.
- [ ] **Dancing Lights** ✱ - Illusion; Bard, Sorcerer, Wizard. Needs: up to four floating lights. nwn5e: in nwn5e (row 1131), borrows no cast visual borrowed in the row; lights per its script.
- [ ] **Druidcraft** ✱ - Transmutation; Druid. Needs: tiny nature effects (bloom, weather sense). nwn5e: in nwn5e (row 1104), borrows cast hand vco_smhannatr01.
- [ ] **Eldritch Blast** - Evocation; Warlock. Needs: crackling force beam, several at higher levels. nwn5e: not in nwn5e.
- [ ] **Elementalism** ✱ - Transmutation; Druid, Sorcerer, Wizard. Needs: small air/earth/fire/water effect at a point. nwn5e: in nwn5e (row 1109), borrows cast hand vco_smhannatr01; VFX_IMP_PULSE_WIND / DUST_EXPLOSION / FLAME_S + FNF_SMOKE_PUFF / PULSE_WATER.
- [ ] **Fire Bolt** ✱ - Evocation; Sorcerer, Wizard. Needs: fire bolt projectile. nwn5e: in nwn5e (row 1050), borrows cast hand vco_mehanfire01; VFX_IMP_MIRV_FLAME flight, VFX_IMP_FLAME_S hit.
- [ ] **Guidance** ✱ - Divination; Cleric, Druid. Needs: touch glow, small buff mark. nwn5e: in nwn5e (row 1057), borrows cast hand vco_smhanelec01; VFX_IMP_HEAD_MIND.
- [ ] **Mage Hand** ✱ - Conjuration; Bard, Sorcerer, Warlock, Wizard. Needs: a spectral hand. nwn5e: in nwn5e (row 1122), borrows cast hand vco_smhanodd01; VFX_IMP_MAGBLUE at the spot.
- [ ] **Mending** - Transmutation; Bard, Cleric, Druid, Sorcerer, Wizard. Needs: sparkle that mends an object. nwn5e: not in nwn5e.
- [ ] **Message** - Transmutation; Bard, Druid, Sorcerer, Wizard. Needs: whisper streak to a creature. nwn5e: not in nwn5e.
- [ ] **Minor Illusion** ✱ - Illusion; Bard, Sorcerer, Warlock, Wizard. Needs: a sound or small image. nwn5e: in nwn5e (row 1123), borrows cast hand vco_smhanmind01; no impact look.
- [ ] **Poison Spray** ✱ - Necromancy; Druid, Sorcerer, Warlock, Wizard. Needs: green gas jet at a target. nwn5e: in nwn5e (row 1053), borrows cast hand vco_mebalacid01; VFX_BEAM_DISINTEGRATE, VFX_IMP_POISON_S.
- [ ] **Prestidigitation** ✱ - Transmutation; Bard, Sorcerer, Warlock, Wizard. Needs: small sparks, smells, marks. nwn5e: in nwn5e (row 1114), borrows cast hand vco_smhanodd01.
- [ ] **Produce Flame** ✱ - Conjuration; Druid. Needs: flame in the hand, thrown. nwn5e: in nwn5e (row 1096), borrows cast hand vco_mehanfire01; VFX_IMP_FLAME_S, VFX_DUR_LIGHT_ORANGE_15.
- [ ] **Sacred Flame** ✱ - Evocation; Cleric. Needs: radiant flame falling on a target. nwn5e: in nwn5e (row 1054), borrows cast hand vco_smhanholy01; VFX_IMP_DIVINE_STRIKE_FIRE.
- [ ] **Shillelagh** ✱ - Transmutation; Druid. Needs: club glows (weapon enchant). nwn5e: in nwn5e (row 1099), borrows cast hand vco_smhannatr01; VFX_IMP_HEAD_NATURE.
- [ ] **Shocking Grasp** ✱ - Evocation; Sorcerer, Wizard. Needs: electric touch. nwn5e: in nwn5e (row 1052), borrows cast hand vco_mebalelec01; VFX_IMP_LIGHTNING_S.
- [ ] **Sorcerous Burst** ✱ - Evocation; Sorcerer. Needs: elemental burst, type chosen. nwn5e: in nwn5e (row 1059), borrows cast hand vco_mehancold03 + vca_outsonic; by damage type VFX_IMP_MIRV / VFX_BEAM_* / VFX_IMP_*_S.
- [ ] **Spare the Dying** ✱ - Necromancy; Cleric, Druid. Needs: healing touch glow. nwn5e: in nwn5e (row 1058), borrows cast hand vco_smhanheal01; VFX_IMP_HEAD_HEAL.
- [ ] **Starry Wisp** ✱ - Evocation; Bard, Druid. Needs: glowing star bolt. nwn5e: in nwn5e (row 1055), borrows cast hand vco_smhanholy01; VFX_IMP_MIRV flight, VFX_IMP_MAGBLUE, VFX_DUR_LIGHT_WHITE_5.
- [ ] **Thaumaturgy** ✱ - Transmutation; Cleric. Needs: booming voice, flickering flames, tremor. nwn5e: in nwn5e (row 1117), borrows no cast visual; VFX_IMP_HEAD_SONIC (Booming Voice).
- [ ] **Vicious Mockery** ✱ - Enchantment; Bard. Needs: psychic sound wave at a target. nwn5e: in nwn5e (row 1056), borrows cast hand vco_smhanmind01, projectile vpr_los; VFX_IMP_HEAD_MIND.

### Level 1 (31)

- [ ] **Alarm** - Abjuration; Ranger, Wizard. Needs: ward area (invisible trigger, ping). nwn5e: not in nwn5e.
- [ ] **Chromatic Orb** ✱ - Evocation; Sorcerer, Wizard. Needs: orb of a chosen element thrown. nwn5e: in nwn5e (row 1153), borrows cast hand vco_mehancold03; per type VFX_IMP_MIRV(_FLAME/_ELECTRIC) + VFX_BEAM_* + VFX_IMP_*_S.
- [ ] **Command** ✱ - Enchantment; Bard, Cleric, Paladin. Needs: one word, mind-affecting mark. nwn5e: in nwn5e (row 1170), borrows cast hand vco_smhanmind01; VFX_DUR_MIND_AFFECTING_DISABLED / _FEAR.
- [ ] **Comprehend Languages** - Divination; Bard, Sorcerer, Warlock, Wizard. Needs: self buff glow. nwn5e: not in nwn5e.
- [ ] **Create or Destroy Water** - Transmutation; Cleric, Druid. Needs: water splash or evaporation at a point. nwn5e: not in nwn5e.
- [ ] **Detect Magic** - Divination; Bard, Cleric, Druid, Paladin, Ranger, Sorcerer, Warlock, Wizard. Needs: self pulse, glowing auras. nwn5e: not in nwn5e.
- [ ] **Detect Poison and Disease** - Divination; Cleric, Druid, Paladin, Ranger. Needs: self pulse. nwn5e: not in nwn5e.
- [ ] **Disguise Self** - Illusion; Bard, Sorcerer, Wizard. Needs: self illusion shimmer. nwn5e: not in nwn5e.
- [ ] **Dissonant Whispers** ✱ - Enchantment; Bard. Needs: psychic whisper at one target. nwn5e: in nwn5e (row 1150), borrows cast hand vco_smhanmind01; VFX_IMP_HEAD_SONIC, VFX_IMP_CONFUSION_S.
- [ ] **Ensnaring Strike** ✱ - Conjuration; Ranger. Needs: vines coil on a weapon hit. nwn5e: in nwn5e (row 1185), borrows cast hand vco_smhannatr01; VFX_DUR_ENTANGLE.
- [ ] **Faerie Fire** ✱ - Evocation; Bard, Druid. Needs: coloured outline glow on creatures in an area. nwn5e: in nwn5e (row 1176), borrows cast hand vco_mehancold03; VFX_IMP_MAGBLUE, VFX_DUR_CESSATE_NEUTRAL.
- [ ] **False Life** ✱ - Necromancy; Sorcerer, Wizard. Needs: self necrotic buff. nwn5e: in nwn5e (row 1149), borrows cast hand vco_smhanevil01; VFX_IMP_EVIL_HELP.
- [ ] **Feather Fall** ✱ - Transmutation; Bard, Sorcerer, Wizard. Needs: wind cushion on falling creatures. nwn5e: in nwn5e (row 1182), borrows cast hand vco_smhanelec01; VFX_IMP_PULSE_WIND.
- [ ] **Floating Disk** - Conjuration; Wizard. Needs: hovering disc of force. nwn5e: not in nwn5e.
- [ ] **Fog Cloud** ✱ - Conjuration; Druid, Ranger, Sorcerer, Wizard. Needs: sphere of fog. nwn5e: in nwn5e (row 1180), borrows cast hand vco_smhanelec01; persistent area vps_fogmind (vfx_persistent 107-115), VFX_DUR_GLOW_BLUE.
- [ ] **Goodberry** - Conjuration; Druid, Ranger. Needs: create berries. nwn5e: not in nwn5e.
- [ ] **Guiding Bolt** ✱ - Evocation; Cleric. Needs: radiant bolt, target glows. nwn5e: in nwn5e (row 1147), borrows cast hand vco_mehanholy03; VFX_BEAM_HOLY, VFX_IMP_SUNSTRIKE, VFX_DUR_LIGHT_WHITE_5.
- [ ] **Healing Word** ✱ - Abjuration; Bard, Cleric, Druid. Needs: quick healing sparkle at range. nwn5e: in nwn5e (row 1151), borrows cast hand vco_smhanheal01; VFX_IMP_HEALING_S.
- [ ] **Hellish Rebuke** - Evocation; Warlock. Needs: reactive hellfire on the attacker. nwn5e: not in nwn5e.
- [ ] **Heroism** ✱ - Enchantment; Bard, Paladin. Needs: touch buff glow. nwn5e: in nwn5e (row 1160), borrows cast hand vco_smhanholy01; VFX_IMP_HEAD_HOLY.
- [ ] **Hex** - Enchantment; Warlock. Needs: curse mark on a target. nwn5e: not in nwn5e.
- [ ] **Hunter's Mark** ✱ - Divination; Ranger. Needs: mark on a target. nwn5e: in nwn5e (row 1162), borrows cast hand vco_smhanmind01; VFX_IMP_HEAD_ODD.
- [ ] **Illusory Script** - Illusion; Bard, Warlock, Wizard. Needs: writing shimmer. nwn5e: not in nwn5e.
- [ ] **Jump** - Transmutation; Druid, Ranger, Sorcerer, Wizard. Needs: touch buff, leaping. nwn5e: not in nwn5e.
- [ ] **Longstrider** ✱ - Transmutation; Bard, Druid, Ranger, Wizard. Needs: touch buff, speed glow. nwn5e: in nwn5e (row 1161), borrows cast hand vco_smhanelec01; VFX_IMP_HASTE.
- [ ] **Purify Food and Drink** - Transmutation; Cleric, Druid, Paladin. Needs: sparkle on items. nwn5e: not in nwn5e.
- [ ] **Ray of Sickness** ✱ - Necromancy; Sorcerer, Wizard. Needs: green sickly ray. nwn5e: in nwn5e (row 1148), borrows cast hand vco_smhanevil01; VFX_BEAM_DISINTEGRATE, VFX_IMP_POISON_S.
- [ ] **Searing Smite** ✱ - Evocation; Paladin. Needs: weapon ignites on a hit. nwn5e: in nwn5e (row 1184), borrows cast hand vco_smhanfire01; VFX_IMP_FLAME_S, VFX_DUR_INFERNO_CHEST.
- [ ] **Silent Image** - Illusion; Bard, Sorcerer, Wizard. Needs: a silent illusion image. nwn5e: not in nwn5e.
- [ ] **Speak with Animals** - Divination; Bard, Druid, Ranger, Warlock. Needs: self buff, talk to beasts. nwn5e: not in nwn5e.
- [ ] **Unseen Servant** - Conjuration; Bard, Warlock, Wizard. Needs: invisible helper (ambient). nwn5e: not in nwn5e.

### Level 2 (31)

- [ ] **Animal Messenger** - Enchantment; Bard, Druid, Ranger. Needs: small creature sent. nwn5e: not in nwn5e.
- [ ] **Arcane Lock** - Abjuration; Wizard. Needs: sealing glyph on a door. nwn5e: not in nwn5e.
- [ ] **Arcanist's Magic Aura** - Illusion; Wizard. Needs: ward over an object (invisible). nwn5e: not in nwn5e.
- [ ] **Augury** - Divination; Cleric, Druid, Wizard. Needs: omen divination (ambient). nwn5e: not in nwn5e.
- [ ] **Blur** - Illusion; Sorcerer, Wizard. Needs: self shimmer. nwn5e: not in nwn5e.
- [ ] **Detect Thoughts** - Divination; Bard, Sorcerer, Wizard. Needs: mind probe pulse. nwn5e: not in nwn5e.
- [ ] **Dragon's Breath** - Transmutation; Sorcerer, Wizard. Needs: touch buff, then a breath cone. nwn5e: not in nwn5e.
- [ ] **Enlarge/Reduce** - Transmutation; Bard, Druid, Sorcerer, Wizard. Needs: target grows or shrinks. nwn5e: not in nwn5e.
- [ ] **Enthrall** - Enchantment; Bard, Warlock. Needs: caster captivates listeners. nwn5e: not in nwn5e.
- [ ] **Flaming Sphere** - Conjuration; Druid, Sorcerer, Wizard. Needs: rolling ball of fire. nwn5e: not in nwn5e.
- [ ] **Gentle Repose** - Necromancy; Cleric, Paladin, Wizard. Needs: ward on a corpse. nwn5e: not in nwn5e.
- [ ] **Heat Metal** - Transmutation; Bard, Druid. Needs: metal glows red hot. nwn5e: not in nwn5e.
- [ ] **Levitate** - Transmutation; Sorcerer, Wizard. Needs: target rises. nwn5e: not in nwn5e.
- [ ] **Locate Animals or Plants** - Divination; Bard, Druid, Ranger. Needs: divination pulse. nwn5e: not in nwn5e.
- [ ] **Locate Object** - Divination; Bard, Cleric, Druid, Paladin, Ranger, Wizard. Needs: divination pulse. nwn5e: not in nwn5e.
- [ ] **Magic Mouth** - Illusion; Bard, Wizard. Needs: ward on an object. nwn5e: not in nwn5e.
- [ ] **Mind Spike** - Divination; Sorcerer, Warlock, Wizard. Needs: psychic bolt, tracking beacon. nwn5e: not in nwn5e.
- [ ] **Mirror Image** - Illusion; Bard, Sorcerer, Warlock, Wizard. Needs: illusory duplicates around the caster. nwn5e: not in nwn5e.
- [ ] **Misty Step** - Conjuration; Sorcerer, Warlock, Wizard. Needs: short teleport, silver mist. nwn5e: not in nwn5e.
- [ ] **Moonbeam** - Evocation; Druid. Needs: cylinder of pale light. nwn5e: not in nwn5e.
- [ ] **Pass without Trace** - Abjuration; Druid, Ranger. Needs: self aura, shadow veil. nwn5e: not in nwn5e.
- [ ] **Phantasmal Force** - Illusion; Bard, Sorcerer, Wizard. Needs: mental illusion on one target. nwn5e: not in nwn5e.
- [ ] **Protection from Poison** - Abjuration; Cleric, Druid, Paladin, Ranger. Needs: touch ward, green shield. nwn5e: not in nwn5e.
- [ ] **Rope Trick** - Transmutation; Wizard. Needs: rope rising into a hidden space. nwn5e: not in nwn5e.
- [ ] **Scorching Ray** - Evocation; Sorcerer, Wizard. Needs: three fire rays. nwn5e: not in nwn5e.
- [ ] **Shining Smite** - Transmutation; Paladin. Needs: weapon hit flares radiant, target outlined. nwn5e: not in nwn5e.
- [ ] **Spider Climb** - Transmutation; Sorcerer, Warlock, Wizard. Needs: touch buff, climb surfaces. nwn5e: not in nwn5e.
- [ ] **Spiritual Weapon** - Evocation; Cleric. Needs: floating spectral weapon. nwn5e: not in nwn5e.
- [ ] **Suggestion** - Enchantment; Bard, Sorcerer, Warlock, Wizard. Needs: whispered mind control. nwn5e: not in nwn5e.
- [ ] **Warding Bond** - Abjuration; Cleric, Paladin. Needs: link between two creatures. nwn5e: not in nwn5e.
- [ ] **Zone of Truth** - Enchantment; Bard, Cleric, Paladin. Needs: area of truth, glyph on the ground. nwn5e: not in nwn5e.

### Level 3 (26)

- [ ] **Beacon of Hope** - Abjuration; Cleric. Needs: aura of hope on allies. nwn5e: not in nwn5e.
- [ ] **Blink** - Transmutation; Sorcerer, Wizard. Needs: self flickers between planes. nwn5e: not in nwn5e.
- [ ] **Conjure Animals** - Conjuration; Druid, Ranger. Needs: summon spectral beasts. nwn5e: not in nwn5e.
- [ ] **Counterspell** - Abjuration; Sorcerer, Warlock, Wizard. Needs: reaction, shatters an incoming spell. nwn5e: not in nwn5e.
- [ ] **Create Food and Water** - Conjuration; Cleric, Paladin. Needs: plates of food and water. nwn5e: not in nwn5e.
- [ ] **Daylight** - Evocation; Cleric, Druid, Paladin, Ranger, Sorcerer. Needs: 20-ft sphere of bright light. nwn5e: not in nwn5e.
- [ ] **Fly** - Transmutation; Sorcerer, Warlock, Wizard. Needs: touch buff, target rises and floats. nwn5e: not in nwn5e.
- [ ] **Gaseous Form** - Transmutation; Sorcerer, Warlock, Wizard. Needs: target becomes mist. nwn5e: not in nwn5e.
- [ ] **Hypnotic Pattern** - Illusion; Bard, Sorcerer, Warlock, Wizard. Needs: swirl of colours in an area. nwn5e: not in nwn5e.
- [ ] **Major Image** - Illusion; Bard, Sorcerer, Warlock, Wizard. Needs: large illusion. nwn5e: not in nwn5e.
- [ ] **Mass Healing Word** - Abjuration; Bard, Cleric. Needs: healing sparkle on several allies. nwn5e: not in nwn5e.
- [ ] **Meld into Stone** - Transmutation; Cleric, Druid, Ranger. Needs: merge into rock. nwn5e: not in nwn5e.
- [ ] **Nondetection** - Abjuration; Bard, Ranger, Wizard. Needs: ward (invisible). nwn5e: not in nwn5e.
- [ ] **Phantom Steed** - Illusion; Wizard. Needs: spectral horse. nwn5e: not in nwn5e.
- [ ] **Plant Growth** - Transmutation; Bard, Druid, Ranger. Needs: area overgrown with plants. nwn5e: not in nwn5e.
- [ ] **Revivify** - Necromancy; Cleric, Druid, Paladin, Ranger. Needs: healing flash on a dead creature. nwn5e: not in nwn5e.
- [ ] **Sending** - Divination; Bard, Cleric, Wizard. Needs: whisper message. nwn5e: not in nwn5e.
- [ ] **Sleet Storm** - Conjuration; Druid, Sorcerer, Wizard. Needs: cylinder of sleet and ice. nwn5e: not in nwn5e.
- [ ] **Speak with Dead** - Necromancy; Bard, Cleric, Wizard. Needs: corpse speaks. nwn5e: not in nwn5e.
- [ ] **Speak with Plants** - Transmutation; Bard, Druid, Ranger. Needs: plants awaken to speak. nwn5e: not in nwn5e.
- [ ] **Spirit Guardians** - Conjuration; Cleric. Needs: spirits orbit the caster, aura. nwn5e: not in nwn5e.
- [ ] **Tiny Hut** - Evocation; Bard, Wizard. Needs: dome of force around the party. nwn5e: not in nwn5e.
- [ ] **Tongues** - Divination; Bard, Cleric, Sorcerer, Warlock, Wizard. Needs: touch buff, babble. nwn5e: not in nwn5e.
- [ ] **Water Breathing** - Transmutation; Druid, Ranger, Sorcerer, Wizard. Needs: touch buff, bubbles. nwn5e: not in nwn5e.
- [ ] **Water Walk** - Transmutation; Cleric, Druid, Ranger, Sorcerer. Needs: touch buff, ripples. nwn5e: not in nwn5e.
- [ ] **Wind Wall** - Evocation; Druid, Ranger. Needs: wall of wind. nwn5e: not in nwn5e.

### Level 4 (17)

- [ ] **Arcane Eye** - Divination; Wizard. Needs: invisible floating eye. nwn5e: not in nwn5e.
- [ ] **Compulsion** - Enchantment; Bard. Needs: forced-movement mind effect. nwn5e: not in nwn5e.
- [ ] **Conjure Woodland Beings** - Conjuration; Druid, Ranger. Needs: summon fey. nwn5e: not in nwn5e.
- [ ] **Control Water** - Transmutation; Cleric, Druid, Wizard. Needs: area of water rising or falling. nwn5e: not in nwn5e.
- [ ] **Dimension Door** - Conjuration; Bard, Sorcerer, Warlock, Wizard. Needs: teleport, portal flash. nwn5e: not in nwn5e.
- [ ] **Divination** - Divination; Cleric, Druid, Wizard. Needs: omen (ambient). nwn5e: not in nwn5e.
- [ ] **Fabricate** - Transmutation; Wizard. Needs: raw material reshaped. nwn5e: not in nwn5e.
- [ ] **Faithful Hound** - Conjuration; Wizard. Needs: ghostly guard dog. nwn5e: not in nwn5e.
- [ ] **Giant Insect** - Conjuration; Druid. Needs: small insects swell to giants. nwn5e: not in nwn5e.
- [ ] **Guardian of Faith** - Conjuration; Cleric. Needs: spectral guardian that blasts. nwn5e: not in nwn5e.
- [ ] **Hallucinatory Terrain** - Illusion; Bard, Druid, Warlock, Wizard. Needs: terrain illusion. nwn5e: not in nwn5e.
- [ ] **Locate Creature** - Divination; Bard, Cleric, Druid, Paladin, Ranger, Wizard. Needs: divination pulse. nwn5e: not in nwn5e.
- [ ] **Private Sanctum** - Abjuration; Wizard. Needs: ward over an area. nwn5e: not in nwn5e.
- [ ] **Resilient Sphere** - Abjuration; Wizard. Needs: shimmering sphere of force around a creature. nwn5e: not in nwn5e.
- [ ] **Secret Chest** - Conjuration; Wizard. Needs: chest to another plane (flash). nwn5e: not in nwn5e.
- [ ] **Stone Shape** - Transmutation; Cleric, Druid, Wizard. Needs: rock reshaped. nwn5e: not in nwn5e.
- [ ] **Vitriolic Sphere** - Evocation; Sorcerer, Wizard. Needs: acid sphere explosion. nwn5e: not in nwn5e.

### Level 5 (22)

- [ ] **Animate Objects** - Transmutation; Bard, Sorcerer, Wizard. Needs: objects come to life. nwn5e: not in nwn5e.
- [ ] **Antilife Shell** - Abjuration; Druid. Needs: shimmering shell around the caster. nwn5e: not in nwn5e.
- [ ] **Commune** - Divination; Cleric. Needs: divine contact (ambient). nwn5e: not in nwn5e.
- [ ] **Commune with Nature** - Divination; Druid, Ranger. Needs: earth senses (ambient). nwn5e: not in nwn5e.
- [ ] **Contact Other Plane** - Divination; Warlock, Wizard. Needs: psychic contact (ambient). nwn5e: not in nwn5e.
- [ ] **Creation** - Illusion; Sorcerer, Wizard. Needs: object conjured from shadow. nwn5e: not in nwn5e.
- [ ] **Dream** - Illusion; Bard, Warlock, Wizard. Needs: sleeping messenger. nwn5e: not in nwn5e.
- [ ] **Geas** - Enchantment; Bard, Cleric, Druid, Paladin, Wizard. Needs: compulsion mark. nwn5e: not in nwn5e.
- [ ] **Hallow** - Abjuration; Cleric. Needs: consecrated area with ward. nwn5e: not in nwn5e.
- [ ] **Mislead** - Illusion; Bard, Warlock, Wizard. Needs: invisibility plus a double. nwn5e: not in nwn5e.
- [ ] **Modify Memory** - Enchantment; Bard, Wizard. Needs: mind effect on a target. nwn5e: not in nwn5e.
- [ ] **Passwall** - Transmutation; Wizard. Needs: passage opens in a wall. nwn5e: not in nwn5e.
- [ ] **Reincarnate** - Necromancy; Druid. Needs: corpse returns in a new body. nwn5e: not in nwn5e.
- [ ] **Scrying** - Divination; Bard, Cleric, Druid, Warlock, Wizard. Needs: view through an eye of magic. nwn5e: not in nwn5e.
- [ ] **Seeming** - Illusion; Bard, Sorcerer, Wizard. Needs: illusion changes appearances. nwn5e: not in nwn5e.
- [ ] **Summon Dragon** - Conjuration; Wizard. Needs: a spectral dragon summoned. nwn5e: not in nwn5e.
- [ ] **Telekinesis** - Transmutation; Sorcerer, Wizard. Needs: invisible force lifting a target. nwn5e: not in nwn5e.
- [ ] **Telepathic Bond** - Divination; Bard, Wizard. Needs: link between allies. nwn5e: not in nwn5e.
- [ ] **Teleportation Circle** - Conjuration; Bard, Sorcerer, Warlock, Wizard. Needs: glowing circle, teleport. nwn5e: not in nwn5e.
- [ ] **Tree Stride** - Conjuration; Druid, Ranger. Needs: step into a tree, out of another. nwn5e: not in nwn5e.
- [ ] **Wall of Force** - Evocation; Wizard. Needs: glimmering wall. nwn5e: not in nwn5e.
- [ ] **Wall of Stone** - Evocation; Druid, Sorcerer, Wizard. Needs: stone wall rises. nwn5e: not in nwn5e.

### Level 6 (20)

- [ ] **Conjure Fey** - Conjuration; Druid. Needs: summon fey. nwn5e: not in nwn5e.
- [ ] **Contingency** - Abjuration; Wizard. Needs: ward stored on the caster. nwn5e: not in nwn5e.
- [ ] **Disintegrate** - Transmutation; Sorcerer, Wizard. Needs: green ray, target becomes dust. nwn5e: not in nwn5e.
- [ ] **Eyebite** - Necromancy; Bard, Sorcerer, Warlock, Wizard. Needs: sickening gaze. nwn5e: not in nwn5e.
- [ ] **Find the Path** - Divination; Bard, Cleric, Druid. Needs: glowing guide. nwn5e: not in nwn5e.
- [ ] **Forbiddance** - Abjuration; Cleric. Needs: ward over an area. nwn5e: not in nwn5e.
- [ ] **Freezing Sphere** - Evocation; Sorcerer, Wizard. Needs: frost globe explosion. nwn5e: not in nwn5e.
- [ ] **Guards and Wards** - Abjuration; Bard, Wizard. Needs: area-wide warding. nwn5e: not in nwn5e.
- [ ] **Heroes' Feast** - Conjuration; Bard, Cleric, Druid. Needs: banquet appears. nwn5e: not in nwn5e.
- [ ] **Instant Summons** - Conjuration; Wizard. Needs: object recalled to hand. nwn5e: not in nwn5e.
- [ ] **Irresistible Dance** - Enchantment; Bard, Wizard. Needs: target dances. nwn5e: not in nwn5e.
- [ ] **Magic Jar** - Necromancy; Wizard. Needs: soul in a container. nwn5e: not in nwn5e.
- [ ] **Mass Suggestion** - Enchantment; Bard, Sorcerer, Wizard. Needs: whispered mind control, several targets. nwn5e: not in nwn5e.
- [ ] **Move Earth** - Transmutation; Druid, Sorcerer, Wizard. Needs: ground shifts. nwn5e: not in nwn5e.
- [ ] **Programmed Illusion** - Illusion; Bard, Wizard. Needs: illusion triggered by a condition. nwn5e: not in nwn5e.
- [ ] **Transport via Plants** - Conjuration; Druid. Needs: portal through plants. nwn5e: not in nwn5e.
- [ ] **Wall of Ice** - Evocation; Wizard. Needs: wall of ice. nwn5e: not in nwn5e.
- [ ] **Wall of Thorns** - Conjuration; Druid. Needs: wall of thorny brush. nwn5e: not in nwn5e.
- [ ] **Wind Walk** - Transmutation; Druid. Needs: turn into mist and clouds, fly. nwn5e: not in nwn5e.
- [ ] **Word of Recall** - Conjuration; Cleric. Needs: teleport to sanctuary. nwn5e: not in nwn5e.

### Level 7 (10)

- [ ] **Forcecage** - Evocation; Bard, Warlock, Wizard. Needs: cage of force. nwn5e: not in nwn5e.
- [ ] **Magnificent Mansion** - Conjuration; Bard, Wizard. Needs: door to a mansion. nwn5e: not in nwn5e.
- [ ] **Mirage Arcane** - Illusion; Bard, Druid, Wizard. Needs: terrain illusion. nwn5e: not in nwn5e.
- [ ] **Plane Shift** - Conjuration; Cleric, Druid, Sorcerer, Warlock, Wizard. Needs: portal to another plane. nwn5e: not in nwn5e.
- [ ] **Project Image** - Illusion; Bard, Wizard. Needs: illusory double. nwn5e: not in nwn5e.
- [ ] **Reverse Gravity** - Transmutation; Druid, Sorcerer, Wizard. Needs: area falls upward. nwn5e: not in nwn5e.
- [ ] **Sequester** - Transmutation; Wizard. Needs: target hidden away. nwn5e: not in nwn5e.
- [ ] **Simulacrum** - Illusion; Wizard. Needs: clone of snow and ice. nwn5e: not in nwn5e.
- [ ] **Symbol** - Abjuration; Bard, Cleric, Druid, Wizard. Needs: glyph trap with effects. nwn5e: not in nwn5e.
- [ ] **Teleport** - Conjuration; Bard, Sorcerer, Wizard. Needs: teleportation flash, target(s). nwn5e: not in nwn5e.

### Level 8 (9)

- [ ] **Animal Shapes** - Transmutation; Druid. Needs: party turns into beasts. nwn5e: not in nwn5e.
- [ ] **Antipathy/Sympathy** - Enchantment; Bard, Druid, Wizard. Needs: aura repels or draws creatures. nwn5e: not in nwn5e.
- [ ] **Befuddlement** - Enchantment; Bard, Druid, Warlock, Wizard. Needs: psychic blast. nwn5e: not in nwn5e.
- [ ] **Clone** - Necromancy; Wizard. Needs: duplicate body. nwn5e: not in nwn5e.
- [ ] **Control Weather** - Transmutation; Cleric, Druid, Wizard. Needs: weather changes (area). nwn5e: not in nwn5e.
- [ ] **Demiplane** - Conjuration; Sorcerer, Warlock, Wizard. Needs: door to a pocket room. nwn5e: not in nwn5e.
- [ ] **Glibness** - Enchantment; Bard, Warlock. Needs: self buff. nwn5e: not in nwn5e.
- [ ] **Maze** - Conjuration; Wizard. Needs: target vanishes into a maze. nwn5e: not in nwn5e.
- [ ] **Tsunami** - Conjuration; Druid. Needs: wall of water sweeps across. nwn5e: not in nwn5e.

### Level 9 (8)

- [ ] **Astral Projection** - Necromancy; Cleric, Warlock, Wizard. Needs: astral silver cords. nwn5e: not in nwn5e.
- [ ] **Foresight** - Divination; Bard, Druid, Warlock, Wizard. Needs: touch buff, glow. nwn5e: not in nwn5e.
- [ ] **Imprisonment** - Abjuration; Warlock, Wizard. Needs: binding in a chain, sphere or coffin. nwn5e: not in nwn5e.
- [ ] **Power Word Heal** - Enchantment; Bard, Cleric. Needs: healing word, flash. nwn5e: not in nwn5e.
- [ ] **Prismatic Wall** - Abjuration; Bard, Wizard. Needs: wall of seven colours. nwn5e: not in nwn5e.
- [ ] **True Polymorph** - Transmutation; Bard, Warlock, Wizard. Needs: target transforms. nwn5e: not in nwn5e.
- [ ] **True Resurrection** - Necromancy; Cleric, Druid. Needs: soul returns, flash. nwn5e: not in nwn5e.
- [ ] **Wish** - Conjuration; Sorcerer, Wizard. Needs: self flash, varied effect. nwn5e: not in nwn5e.

## Check by hand

Not counted above: NWN has something close, but maybe not the same spell. Decide each one in game.

- [ ] **Alter Self** (Level 2, Transmutation; Sorcerer, Wizard): NWN row 130 Polymorph Self is a transformation; 2024 Alter Self is a mild body change. Related, not the same. nwn5e: not in nwn5e.
- [ ] **Animal Friendship** ✱ (Level 1, Enchantment; Bard, Druid, Ranger): NWN has Charm Person or Animal (row 17, level 2; charms animals) but nothing by this name. Same spell in effect? nwn5e added its own row 1175. nwn5e: in nwn5e (row 1175), borrows cast hand vco_smhanmind01; VFX_DUR_MIND_AFFECTING_*.
- [ ] **Antimagic Field** (Level 8, Abjuration; Cleric, Wizard): NWN has only a monster row 727 Beholder_Anti_Magic_Cone; no player spell. Probably not in NWN. nwn5e: not in nwn5e.
- [ ] **Aura of Life** (Level 4, Abjuration; Cleric, Paladin): NWN 372 Aura of Vitality is a different spell. Probably not in NWN (a paladin aura giving necrotic resistance). nwn5e: not in nwn5e.
- [ ] **Blight** (Level 4, Necromancy; Druid, Sorcerer, Warlock, Wizard): NWN has Harm (77) and Evil Blight (566, a prestige-class power). 2024 Blight is necrotic damage to a creature or plant. Probably not in NWN. nwn5e: not in nwn5e.
- [ ] **Calm Emotions** (Level 2, Enchantment; Bard, Cleric): NWN row 12 Calm_Emotions exists in spells.2da but has no class levels (a leftover?). Does it have working VFX? Test in game. nwn5e: not in nwn5e.
- [ ] **Conjure Celestial** (Level 7, Conjuration; Cleric): NWN 379 Summon_Celestial exists (a summon granted by a feature/item). Probably a match on the summon visual. nwn5e: not in nwn5e.
- [ ] **Conjure Elemental** (Level 5, Conjuration; Druid, Wizard): NWN 48 Elemental Swarm (9th) and 378 Summon Mephit; 2024 Conjure Elemental is one elemental. Near. nwn5e: not in nwn5e.
- [ ] **Conjure Minor Elementals** (Level 4, Conjuration; Druid, Wizard): NWN 48 Elemental Swarm / 502 ElementalSummoningItem. Near; the summon visual exists. nwn5e: not in nwn5e.
- [ ] **Darkvision** (Level 2, Transmutation; Druid, Ranger, Sorcerer, Wizard): NWN row 365 Ultravision (level 2) is the same effect (see in the dark) under another name. Likely a match. nwn5e: not in nwn5e.
- [ ] **Detect Evil and Good** (Level 1, Divination; Cleric, Paladin): NWN row 312 Detect_Evil is a paladin class feature, not a spell; no Detect Good. Probably not in NWN. nwn5e: not in nwn5e.
- [ ] **Dispel Evil and Good** (Level 5, Abjuration; Cleric, Paladin): NWN 40 Dismissal and 430 Banishment send outsiders away; 2024 spell also gives disadvantage to listed creatures' attacks. Near, not the same. nwn5e: not in nwn5e.
- [ ] **Divine Smite** ✱ (Level 1, Evocation; Paladin): NWN 315 Smite_Evil is a paladin feat row, not this spell; 2024 Divine Smite is a spell (nwn5e: row 1183). The weapon-strike flash exists, the spell doesn't. nwn5e: in nwn5e (row 1183), borrows cast hand vco_smhanholy01; VFX_IMP_SUNSTRIKE.
- [ ] **Enhance Ability** (Level 2, Transmutation; Bard, Cleric, Druid, Ranger, Sorcerer, Wizard): NWN has the six 3.5e buffs (Bull's Strength 9, Cat's Grace 13, Endurance 49, Eagle's Splendor 354, Fox's Cunning 356, Owl's Wisdom 355), which together cover 2024 Enhance Ability's choices. Likely a match. nwn5e: not in nwn5e.
- [ ] **Find Familiar** (Level 1, Conjuration; Wizard): NWN row 318 Summon_Familiar is the familiar class feature, not a ritual spell. Probably not in NWN as a spell, but a summon visual exists. nwn5e: not in nwn5e.
- [ ] **Find Steed** (Level 2, Conjuration; Paladin): NWN row 818 Paladin_Summon_Mount (and 317 Summon_Animal_Companion) are class features that summon a mount; the summon visual exists. nwn5e: not in nwn5e.
- [ ] **Fire Shield** (Level 4, Evocation; Druid, Sorcerer, Wizard): NWN 47 Elemental Shield (fire/cold shield on you) is the 3.5e equivalent. Likely a match. nwn5e: not in nwn5e.
- [ ] **Flame Blade** (Level 2, Evocation; Druid, Sorcerer): NWN 542 Flame Weapon (weapon buff) and 60 Flame Lash. 2024 Flame Blade conjures a fire blade in hand. Near, not the same. nwn5e: not in nwn5e.
- [ ] **Insect Plague** (Level 5, Conjuration; Cleric, Druid, Sorcerer): NWN 364 Creeping Doom (swarms in an area) is a related swarm area spell; 2024 Insect Plague is a locust cloud. Near, not the same. nwn5e: not in nwn5e.
- [ ] **Magic Circle** (Level 3, Abjuration; Cleric, Paladin, Warlock, Wizard): NWN has Magic Circle against Chaos/Evil/Good/Law (103-106, 322). 2024 Magic Circle is one spell (celestials, elementals, fey, fiends, undead). Same family, different protections. nwn5e: not in nwn5e.
- [ ] **Mass Cure Wounds** (Level 5, Abjuration; Bard, Cleric, Druid): NWN 80 Healing Circle (heal everyone in a pulse) is the 3.5e equivalent; 114 Mass Heal is higher level. Likely a match with Healing Circle. nwn5e: not in nwn5e.
- [ ] **Prayer of Healing** (Level 2, Abjuration; Cleric, Paladin): NWN 133 Prayer is a different spell (party buff); 80 Healing Circle (heal pulse) is closer. Check the visual in game. nwn5e: not in nwn5e.
- [ ] **Shatter** (Level 2, Evocation; Bard, Sorcerer, Wizard): NWN row 167 Sound Burst (level 2, sonic burst area) is the closest; 2024 Shatter is a thunder sphere. Probably a match for VFX purposes. nwn5e: not in nwn5e.

## Counts

| level | SRD total | in NWN | check by hand | not in NWN (listed) | of which in nwn5e | not in nwn5e |
|---|---|---|---|---|---|---|
| Cantrips | 27 | 5 | 0 | 22 | 19 | 3 |
| Level 1 | 57 | 22 | 4 | 31 | 15 | 16 |
| Level 2 | 57 | 18 | 8 | 31 | 0 | 31 |
| Level 3 | 42 | 15 | 1 | 26 | 0 | 26 |
| Level 4 | 34 | 13 | 4 | 17 | 0 | 17 |
| Level 5 | 38 | 12 | 4 | 22 | 0 | 22 |
| Level 6 | 31 | 11 | 0 | 20 | 0 | 20 |
| Level 7 | 20 | 9 | 1 | 10 | 0 | 10 |
| Level 8 | 17 | 7 | 1 | 9 | 0 | 9 |
| Level 9 | 16 | 8 | 0 | 8 | 0 | 8 |
| **Total** | **339** | **120** | **23** | **196** | **34** | **162** |

## Appendix: NWN spells matched by rename or split (not listed above)

- Acid Arrow -> NWN row 115 Melf's Acid Arrow
- Arcane Hand -> NWN row 459-463 Bigby's hands
- Arcane Sword -> NWN row 123 Mordenkainen's Sword
- Black Tentacles -> NWN row 375 Evard's Black Tentacles
- Clairvoyance -> NWN row 20 Clairaudience and Clairvoyance
- Cure Wounds -> NWN row 32 Cure Light Wounds (+33-35)
- Divine Word -> NWN row 194 Word of Faith
- Dominate Beast -> NWN row 43 Dominate Animal
- Greater Invisibility -> NWN row 88 Improved Invisibility
- Hideous Laughter -> NWN row 457 Tasha's Hideous Laughter
- Ice Knife -> NWN row 543 Ice Dagger
- Inflict Wounds -> NWN row 432 Inflict Light Wounds
- Magic Weapon -> NWN row 109/544
- Polymorph -> NWN row 130 Polymorph Self
- Protection from Energy -> NWN row 137 Protection from Elements
- Protection from Evil and Good -> NWN row 138/139 (+321)
- Thunderwave -> NWN row 436 Balagarn's Iron Horn
Same-name matches (NWN row): Acid Splash (424), Aid (1), Animate Dead (2), Awaken (363), Bane (449), Banishment (430), Barkskin (3), Bestow Curse (4), Blade Barrier (5), Bless (6), Blindness/Deafness (8), Burning Hands (10), Call Lightning (11), Chain Lightning (14), Charm Monster (15), Charm Person (16), Circle of Death (18), Cloudkill (23), Color Spray (24), Cone of Cold (25), Confusion (26), Contagion (27), Continual Flame (419), Create Undead (30), Darkness (36), Death Ward (38), Delayed Blast Fireball (39), Dispel Magic (41), Divine Favor (414), Dominate Monster (44), Dominate Person (45), Earthquake (426), Entangle (53), Etherealness (443), Expeditious Retreat (456), Fear (54), Find Traps (377), Finger of Death (56), Fireball (58), Fire Storm (57), Flame Strike (61), Flesh to Stone (485), Freedom of Movement (62), Gate (63), Globe of Invulnerability (65), Glyph of Warding (549), Grease (66), Greater Restoration (70), Gust of Wind (75), Harm (77), Haste (78), Heal (79), Hold Monster (82), Hold Person (83), Holy Aura (84), Ice Storm (368), Identify (86), Incendiary Cloud (89), Invisibility (90), Knock (93), Legend Lore (376), Lesser Restoration (97), Light (100), Lightning Bolt (101), Mage Armor (102), Magic Missile (107), Mass Heal (114), Meteor Swarm (116), Mind Blank (117), Phantasmal Killer (127), Planar Ally (451), Planar Binding (128), Power Word Kill (131), Power Word Stun (132), Prismatic Spray (135), Raise Dead (142), Ray of Enfeeblement (143), Ray of Frost (144), Regenerate (374), Remove Curse (146), Resistance (151), Resurrection (153), Sanctuary (154), See Invisibility (157), Shapechange (161), Shield (417), Shield of Faith (450), Silence (163), Sleep (165), Slow (166), Spike Growth (454), Stinking Cloud (171), Stoneskin (172), Storm of Vengeance (173), Sunbeam (183), Sunburst (427), Time Stop (185), True Seeing (186), True Strike (415), Vampiric Touch (188), Wall of Fire (191), Web (192), Weird (193).

## Sources

- **SRD 5.2 spell list:** the official "System Reference Document 5.2" (CC-BY) PDF, https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.pdf, read with pdftotext (Spell Descriptions section: all 339 spell headers with name, level, school, classes). The repo holds no SRD spell list (docs/, data/, tests/), so the PDF is the source. Class lists below are the ones the SRD prints.
- **NWN spells.2da:** `C:\Users\Bruno\Desktop\Projects\nwn5e-ee\build\served\spells.2da` - the table the DLL writes at start (the game's own rows plus nwn5e's). Rows 0-999 are the game's (labels read by hand); rows 1000+ and a few rewritten base rows (10, 32, 107, 432, 436, 543 ...) are nwn5e's. No loose pristine copy of the base table exists on the machine (the game install holds BIFs only; unpacking them was out of scope), so labels were matched against this served copy; nwn5e only changes columns of base rows, not their labels.
- **nwn5e files read:** `nwn5e-ee\CLAUDE.md`; `data\rows\spells.*.txt` (all spell row files, incl. `spells.if_spells2024.*`, `spells.if_cantrips2024.*`) and `vfx_persistent.if_spells2024.txt`; `src\core\rules_spells2024.cpp`, `rules_cantrips.cpp`, `rules_utility_cantrips.cpp`, `rules_weapon_cantrips.cpp`; `src\rules\spells2024.cpp`; `build\spellbench\looks\spells.look.txt`. No `private\` folder was read. The worktree nwn5e-ee-anim has the same row file list.
- "In nwn5e (row N), borrows ..." = the cast-hand visual (vco_*) from the spells.2da row plus the VFX_* constants in the spell's script.
- Matching is by spell (renames and 3.5e names mapped; see the appendix). A spell NWN has is not listed even if nwn5e has not re-implemented it yet.

---

This work includes material from the System Reference Document 5.2 ("SRD 5.2") by Wizards of the Coast LLC,
available at https://www.dndbeyond.com/srd. The SRD 5.2 is licensed under the Creative Commons Attribution 4.0
International License, available at https://creativecommons.org/licenses/by/4.0/legalcode.
