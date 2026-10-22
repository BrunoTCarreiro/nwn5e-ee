"""Writes data/rows/packft*.txt: the 2024 feats appended to the feat tables
of the base classes' player packages (docs/CHARGEN.md C3).

The packages' default picks (the creation screen's Recommended / OK, the
level-up screen's Recommended) take the first feats of their table that the
character can choose. In faithful mode only the 2024 feats can be chosen
(src/core/rules_feats.cpp), so NWN's own rows are passed over and these are
taken: Ability Score Improvement's choices (C7a: from level 4, the package's
main ability first), a Fighter's Fighting Style
(Defense first for a Strength package, Archery first for a Dexterity one),
the Weapon Masteries of the kit's weapons (C4: the classes with Weapon
Mastery; more after them for the masteries later levels add), Alert (a
human's extra level-1 feat; Savage Attacker or Skilled first for the classes
they suit). Appended ("+" rows), so NWN mode reaches them
only after its own.

Run from the repo root:  python tools/data/packft_rows.py
"""

import os

# The FeatPref2DA tables of packages.2da's player packages of classes 0-10,
# and whether a Fighter package leans on Dexterity (its Attribute column).
TABLES = (
    [f'PackFTBarb{i}' for i in range(1, 6)]
    + [f'PackFTBard{i}' for i in range(1, 6)]
    + [f'PackFTCler{i}' for i in range(1, 7)]
    + [f'PackFTDruid{i}' for i in range(1, 7)]
    + [f'PackFTFight{i}' for i in range(1, 7)]
    + [f'PackFTMonk{i}' for i in range(1, 6)]
    + [f'PackFTPala{i}' for i in range(1, 5)]
    + [f'PackFTRang{i}' for i in range(1, 6)]
    + ['PackFTRog1', 'PackFTRog2', 'PackFTRog3', 'PackFTRog5']
    + [f'PackFTSor{i}' for i in range(1, 10)]
    + [f'PackFTWiz{i}' for i in range(1, 10)]
)
DEX_FIGHTERS = {'PackFTFight2', 'PackFTFight6'}  # Fighter_Finesse, Fighter_WeaponMaster
# Fighter_Pirate's table, also the 2024 greatsword package's (tools/data/kits.py).
GREAT_WEAPON_FIGHTERS = {'PackFTFight3'}

# C7a: each ASI's choices (tools/data/asi_feats.py: row 1400 + 21 (k - 1) +
# split), the package's in order: +2 to its main ability, +1 main / +1
# second, +2 second, +1 second / +1 third. nwn5e offers only the next ASI's
# choices and none that changes nothing (its abilities at 20), so the first
# that fits is taken.
SHORT = ['STR', 'DEX', 'CON', 'INT', 'WIS', 'CHA']
ABILITY_ORDER = {  # main, second, third — as rules::AsiPriority2024
    'PackFTBarb': (0, 2, 1), 'PackFTBard': (5, 1, 2), 'PackFTCler': (4, 2, 0), 'PackFTDruid': (4, 2, 1),
    'PackFTFight': (0, 2, 1), 'PackFTMonk': (1, 4, 2), 'PackFTPala': (0, 5, 2), 'PackFTRang': (1, 4, 2),
    'PackFTRog': (1, 2, 4), 'PackFTSor': (5, 2, 1), 'PackFTWiz': (3, 2, 1),
}


def split_index(a, b=None):
    if b is None:
        return a
    a, b = min(a, b), max(a, b)
    pairs = [(x, y) for x in range(6) for y in range(x + 1, 6)]
    return 6 + pairs.index((a, b))


def asi_rows(table):
    order = next(v for k, v in ABILITY_ORDER.items() if table.startswith(k))
    if table in DEX_FIGHTERS:
        order = (1, 2, 0)
    main, second, third = order
    picks = [(main, None), (main, second), (second, None), (second, third)]
    rows = []
    for slot in range(1, 8):
        for a, b in picks:
            split = split_index(a, b)
            label = SHORT[a] if b is None else '_'.join(SHORT[x] for x in sorted((a, b)))
            rows.append((1400 + 21 * (slot - 1) + split, f'NWN5E_ASI_{slot}_{label}'))
    return rows
STYLES = {
    'archery': (1210, 'NWN5E_FS_ARCHERY'),
    'defense': (1211, 'NWN5E_FS_DEFENSE'),
    'great': (1212, 'NWN5E_FS_GREAT_WEAPON'),
    'two': (1213, 'NWN5E_FS_TWO_WEAPON'),
}
ALERT = (377, 'FEAT_IMPROVED_INITIATIVE')
SAVAGE_ATTACKER = (1300, 'NWN5E_SAVAGE_ATTACKER')
SKILLED = (1301, 'NWN5E_SKILLED')
# A human's Versatile pick (an Origin feat), by class: the first the character
# can still take (Alert last, the fallback).
ORIGIN = {
    'PackFTBarb': [SAVAGE_ATTACKER], 'PackFTFight': [SAVAGE_ATTACKER], 'PackFTPala': [SAVAGE_ATTACKER],
    'PackFTRang': [SAVAGE_ATTACKER], 'PackFTMonk': [SAVAGE_ATTACKER],
    'PackFTRog': [SKILLED], 'PackFTBard': [SKILLED],
}

# C4: the Weapon Masteries a package picks, in order (its kit's weapons first).
MASTERIES = {
    'PackFTBarb': ['Greataxe', 'Handaxe', 'Throwing Axe', 'Greatsword'],
    'PackFTPala': ['Longsword', 'Throwing Axe', 'Greatsword'],
    'PackFTRang': ['Scimitar', 'Longbow', 'Short Sword'],
    'PackFTRog': ['Short Sword', 'Shortbow', 'Dagger', 'Rapier'],
}
FIGHTER_MASTERIES = {
    'great': ['Greatsword', 'Light Flail', 'Throwing Axe', 'Longsword', 'Longbow', 'Halberd'],
    'dex': ['Scimitar', 'Short Sword', 'Longbow', 'Rapier', 'Dagger', 'Light Crossbow'],
    'str': ['Longsword', 'Greatsword', 'Longbow', 'Light Flail', 'Halberd', 'Throwing Axe'],
}


def mastery_rows(names):
    import mastery_feats  # same folder
    ids = {name: mastery_feats.FIRST_FEAT + i for i, (_, name, _, _, _) in enumerate(mastery_feats.WEAPONS)}
    return [(ids[name], mastery_feats.label(name)) for name in names]


def rows_for(table):
    rows = asi_rows(table)
    if table.startswith('PackFTFight'):
        if table in DEX_FIGHTERS:
            order = ['archery', 'defense', 'two', 'great']
        elif table in GREAT_WEAPON_FIGHTERS:
            order = ['great', 'defense', 'two', 'archery']
        else:
            order = ['defense', 'great', 'two', 'archery']
        rows += [STYLES[name] for name in order]
        kind = 'dex' if table in DEX_FIGHTERS else 'great' if table in GREAT_WEAPON_FIGHTERS else 'str'
        rows += mastery_rows(FIGHTER_MASTERIES[kind])
    for prefix, names in MASTERIES.items():
        if table.startswith(prefix):
            rows += mastery_rows(names)
    for prefix, feats in ORIGIN.items():
        if table.startswith(prefix):
            rows += feats
    rows.append(ALERT)
    return rows


def main():
    out = os.path.join('data', 'rows')
    for table in TABLES:
        lines = [
            f'# {table.lower()}.2da - C3 (docs/CHARGEN.md): the 2024 feats appended for the',
            '# package\'s default picks (generated by tools/data/packft_rows.py).',
            'FeatIndex Label',
        ]
        lines += [f'+ {feat} {label}' for feat, label in rows_for(table)]
        with open(os.path.join(out, table.lower() + '.txt'), 'w', encoding='utf-8', newline='\n') as f:
            f.write('\n'.join(lines) + '\n')
    print(f'{len(TABLES)} tables written to {out}')


if __name__ == '__main__':
    main()
