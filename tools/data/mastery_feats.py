"""Writes the Weapon Mastery feats (docs/CHARGEN.md C4):

- data/rows/feat.mastery.txt: one feat per weapon kind, rows 1260 + the
  kind's place in rules::NwnMasteryWeapons() (src/rules/weapons.cpp — the
  same order: append only, the ids are saved in characters), each a
  sub-feat of the "Weapon Mastery" master feat, needing one of the weapon's
  proficiency feats (OrReqFeat0-4: the engine checks it);
- data/rows/masterfeats.mastery.txt: the master feat (row 44);
- data/rows/cls_feat_<class>.mastery.txt: the feats in the bonus lists of the
  five classes with Weapon Mastery (List 2);
- their strings, between the generated markers in data/strings.txt.

tests/data/mastery_feats_test.cpp checks the rows against the C++ table.
Run from the repo root:  python tools/data/mastery_feats.py
"""

import io
import os

FIRST_FEAT = 1260
MASTER_ROW = 44
STR_MASTER = 16000330  # name, then description
STR_PROPERTY = 16000332  # Cleave .. Vex, in PROPERTIES order
STR_WEAPON = 16000340  # one name per weapon

PROPERTIES = ['Cleave', 'Graze', 'Nick', 'Push', 'Sap', 'Slow', 'Topple', 'Vex']
PROPERTY_TEXT = {
    'Cleave': "If you hit a creature with a melee attack with this weapon, you can attack a second creature within "
              "5 feet of the first and within your reach; on a hit it takes the weapon's damage without your ability "
              "modifier (unless it is negative). Once per turn.\\n\\nIn nwn5e: the game's cleave attack, once per round.",
    'Graze': "If your attack with this weapon misses a creature, it still takes damage equal to the ability modifier "
             "you attacked with.",
    'Nick': "The extra attack of fighting with two light weapons is part of your Attack action, not a Bonus Action. "
            "Once per turn.\\n\\nIn nwn5e: the game's off-hand attack always is.",
    'Push': "If you hit a creature of up to Large size with this weapon, you can push it up to 10 feet straight away "
            "from you.",
    'Sap': "If you hit a creature with this weapon, it has Disadvantage on its next attack roll before the start of "
           "your next turn.",
    'Slow': "If you hit a creature with this weapon and deal damage, its Speed drops by 10 feet until the start of "
            "your next turn (not more, however many hits).\\n\\nIn nwn5e: its speed drops by a third for a round.",
    'Topple': "If you hit a creature with this weapon, it makes a Constitution saving throw (DC 8 + the ability "
              "modifier you attacked with + your Proficiency Bonus) or falls Prone.\\n\\nIn nwn5e: knocked down for "
              "a round.",
    'Vex': "If you hit a creature with this weapon and deal damage, you have Advantage on your next attack roll "
           "against it before the end of your next turn.",
}
MASTER_TEXT = (
    "Weapon Mastery (2024): your training lets you use the mastery property of the kinds of weapons you choose, "
    "among those you are proficient with. Fighters choose 3 at level 1 (4 at Fighter 4, 5 at 10, 6 at 16), "
    "Barbarians 2 (3 at 4, 4 at 10), Paladins, Rangers and Rogues 2.\\n\\nAfter a long rest you can swap one: "
    "!mastery <weapon you have> for <new weapon>.")

# The rules::Weapons() table: base item, name, mastery, Weapon Focus icon, proficiency feats.
WEAPONS = [
    (22, 'Dagger', 'Nick', 'ife_wepfoc_Dag', [46, 48, 49, 50, 51]),
    (28, 'Club', 'Slow', 'ife_wepfoc_Clu', [46, 48, 49, 50, 51]),
    (9, 'Light Mace', 'Sap', 'ife_wepfoc_Lma', [46, 50]),
    (47, 'Morningstar', 'Sap', 'ife_wepfoc_Mor', [46, 50]),
    (50, 'Quarterstaff', 'Topple', 'ife_wepfoc_Sta', [46, 48, 49, 50, 51]),
    (58, 'Spear', 'Sap', 'ife_wepfoc_Spe', [46, 48]),
    (60, 'Sickle', 'Nick', 'ife_wepfoc_Sic', [46, 48]),
    (7, 'Light Crossbow', 'Slow', 'ife_wepfoc_LXb', [46, 49, 50, 51]),
    (6, 'Heavy Crossbow', 'Push', 'ife_wepfoc_Hxb', [46, 49, 50, 51]),
    (31, 'Dart', 'Vex', 'ife_wepfoc_Dar', [46, 48, 50]),
    (61, 'Sling', 'Slow', 'ife_wepfoc_SLi', [46, 48, 49, 50]),
    (0, 'Short Sword', 'Vex', 'ife_wepfoc_Ssw', [45, 50]),
    (1, 'Longsword', 'Sap', 'ife_wepfoc_LSw', [45, 256]),
    (2, 'Battleaxe', 'Topple', 'ife_wepfoc_Bax', [45]),
    (4, 'Light Flail', 'Sap', 'ife_wepfoc_Lfl', [45]),
    (5, 'Warhammer', 'Push', 'ife_wepfoc_Wha', [45]),
    (10, 'Halberd', 'Cleave', 'ife_wepfoc_Hal', [45]),
    (13, 'Greatsword', 'Graze', 'ife_wepfoc_Gsw', [45]),
    (18, 'Greataxe', 'Cleave', 'ife_wepfoc_Gax', [45]),
    (35, 'Heavy Flail', 'Topple', 'ife_wepfoc_Hfl', [45]),
    (37, 'Light Hammer', 'Nick', 'ife_wepfoc_LHa', [45]),
    (38, 'Handaxe', 'Vex', 'ife_wepfoc_Hax', [45, 49, 50]),
    (51, 'Rapier', 'Vex', 'ife_wepfoc_Rap', [45, 50, 256]),
    (53, 'Scimitar', 'Nick', 'ife_wepfoc_Sci', [45, 48]),
    (63, 'Throwing Axe', 'Vex', 'ife_wepfoc_Tax', [45]),
    (95, 'Trident', 'Topple', 'ife_wepfoc', [45]),
    (8, 'Longbow', 'Slow', 'ife_wepfoc_Lbw', [45, 256]),
    (11, 'Shortbow', 'Vex', 'ife_wepfoc_Sbw', [45, 50, 256]),
    (92, 'Lance', 'Topple', 'ife_wepfoc', []),
    (3, 'Bastard Sword', 'Sap', 'ife_wepfoc_Bsw', [44]),
    (12, 'Two-Bladed Sword', 'Cleave', 'ife_wepfoc_2sw', [44]),
    (32, 'Dire Mace', 'Cleave', 'ife_wepfoc_Dma', [44]),
    (33, 'Double Axe', 'Cleave', 'ife_wepfoc_Dax', [44]),
    (40, 'Kama', 'Nick', 'ife_wepfoc_Kam', [44, 49]),
    (41, 'Katana', 'Sap', 'ife_wepfoc_Kat', [44]),
    (42, 'Kukri', 'Nick', 'ife_wepfoc_Kuk', [44]),
    (55, 'Scythe', 'Graze', 'ife_wepfoc_Scy', [44]),
    (59, 'Shuriken', 'Vex', 'ife_wepfoc_Shu', [44, 49]),
    (108, 'Dwarven Waraxe', 'Topple', 'ife_X2WFDWAx', [44]),
    (111, 'Whip', 'Slow', 'ife_X2WFWhip', [44]),
]
MASTERY_CLASSES = ['barb', 'fight', 'pal', 'rang', 'rog']
BEGIN = '# BEGIN generated by tools/data/mastery_feats.py - edit the tool, not these lines'
END = '# END generated by tools/data/mastery_feats.py'


def label(name):
    return 'NWN5E_WM_' + name.upper().replace(' ', '_').replace('-', '_')


def main():
    rows = os.path.join('data', 'rows')
    feat_lines = [
        '# feat.2da - C4 (docs/CHARGEN.md): Weapon Mastery, one feat per weapon kind (generated by',
        '# tools/data/mastery_feats.py; the ids follow rules::NwnMasteryWeapons(), append only).',
        'LABEL FEAT DESCRIPTION ICON GAINMULTIPLE EFFECTSSTACK ALLCLASSESCANUSE CRValue MASTERFEAT OrReqFeat0 '
        'OrReqFeat1 OrReqFeat2 OrReqFeat3 OrReqFeat4 Constant TOOLSCATEGORIES PreReqEpic ReqAction',
    ]
    for i, (base, name, mastery, icon, proficiency) in enumerate(WEAPONS):
        req = [str(f) for f in proficiency] + ['****'] * (5 - len(proficiency))
        feat_lines.append(f'{FIRST_FEAT + i} {label(name)} {STR_WEAPON + i} {STR_PROPERTY + PROPERTIES.index(mastery)} '
                          f'{icon} 0 0 0 1 {MASTER_ROW} {" ".join(req)} FEAT_{label(name)} 6 0 1')
    with open(os.path.join(rows, 'feat.mastery.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(feat_lines) + '\n')
    with open(os.path.join(rows, 'masterfeats.mastery.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('# masterfeats.2da - C4: Weapon Mastery, the master of feat.mastery.txt (generated).\n'
                'LABEL STRREF DESCRIPTION ICON\n'
                f'{MASTER_ROW} NWN5E_WEAPON_MASTERY {STR_MASTER} {STR_MASTER + 1} ife_wepfoc\n')
    for cls in MASTERY_CLASSES:
        lines = [f'# cls_feat_{cls}.2da - C4: the Weapon Mastery feats in this class\'s bonus list (List 2)',
                 '# (generated by tools/data/mastery_feats.py).',
                 'FeatLabel FeatIndex List GrantedOnLevel OnMenu']
        lines += [f'+ {label(name)} {FIRST_FEAT + i} 2 -1 0' for i, (_, name, _, _, _) in enumerate(WEAPONS)]
        with open(os.path.join(rows, f'cls_feat_{cls}.mastery.txt'), 'w', encoding='utf-8', newline='\n') as f:
            f.write('\n'.join(lines) + '\n')
    strings = [BEGIN,
               f'{STR_MASTER} Weapon Mastery',
               f'{STR_MASTER + 1} {MASTER_TEXT}']
    for i, prop in enumerate(PROPERTIES):
        strings.append(f'{STR_PROPERTY + i} Weapon Mastery (2024): {prop}. {PROPERTY_TEXT[prop]}')
    for i, (_, name, mastery, _, _) in enumerate(WEAPONS):
        strings.append(f'{STR_WEAPON + i} Weapon Mastery: {name} ({mastery})')
    strings.append(END)
    for line in strings:
        assert all(ord(c) < 128 for c in line), line
    path = os.path.join('data', 'strings.txt')
    text = io.open(path, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in text else '\n'
    block = nl.join(strings)
    if BEGIN in text:
        start = text.index(BEGIN)
        end = text.index(END, start) + len(END)
        text = text[:start] + block + text[end:]
    else:
        if not text.endswith(nl):
            text += nl
        text += block + nl
    io.open(path, 'w', encoding='utf-8', newline='').write(text)
    print(f'{len(WEAPONS)} mastery feats ({FIRST_FEAT}-{FIRST_FEAT + len(WEAPONS) - 1}) written')


if __name__ == '__main__':
    main()
