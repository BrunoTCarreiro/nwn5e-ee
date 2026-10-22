"""Writes C5's data (docs/CHARGEN.md): the creation screen's Skills page as the
2024 pick, faithful.

- data/rows/skills.added.txt: skills.2da rows 28-34, the 2024 skills NWN has no row
  for (Athletics, History, Insight, Investigation, Nature, Religion, Survival).
  They exist in both modes, so saves load either way; NWN mode leaves them
  unusable and off the level-up page.
- data/rows/skills.if_skills2024*.txt (served with the Skills page hooks, after
  skills.added.txt: a table's row files apply in name order,
  src/core/chargen_skills.cpp): the new rows usable by every class, and the
  page showing one row per 2024 skill (rules::SkillsPageRow) — the others
  hidden from it (still on the character sheet).
- data/files/cls_sk5e_*.2da: the base classes' skill tables with the 2024
  class lists as class skills (rules::ClassSkillsFor; a Bard's: every skill),
  rows without a 2024 skill and the tool rows keeping NWN's flag, the same
  skills usable as in NWN (Animal Empathy, Perform, Use Magic Device stay
  where NWN has them) plus the new rows.
- data/rows/classes.if_skills2024.txt: classes 0-10 point at those tables.

tests/data/class_skills_test.cpp checks the tables against the rules.
Run from the repo root:  python tools/data/class_skills.py
"""

import os

# skills.2da rows 0-27 (NWN's labels) and the 2024 skill each stands for
# (rules::Skill2024ForNwnSkill); None = no 2024 skill, 'tool' = a tool.
NWN_ROWS = [
    ('AnimalEmpathy', 'Animal Handling'), ('Concentration', None), ('DisableTrap', 'tool'),
    ('Discipline', None), ('Heal', 'Medicine'), ('Hide', 'Stealth'), ('Listen', 'Perception'),
    ('Lore', 'Arcana'), ('MoveSilently', 'Stealth'), ('OpenLock', 'tool'), ('Parry', None),
    ('Perform', 'Performance'), ('Persuade', 'Persuasion'), ('PickPocket', 'Sleight of Hand'),
    ('Search', 'Perception'), ('SetTrap', 'tool'), ('Spellcraft', 'Arcana'), ('Spot', 'Perception'),
    ('Taunt', None), ('UseMagicDevice', None), ('Appraise', None), ('Tumble', 'Acrobatics'),
    ('CraftTrap', None), ('Bluff', 'Deception'), ('Intimidate', 'Intimidation'), ('CraftArmor', 'tool'),
    ('CraftWeapon', 'tool'), ('Ride', 'Animal Handling'),
]
# nwn5e's rows 28-34: (label, 2024 skill, key ability, icon, name strref).
NEW_ROWS = [
    ('Athletics', 'Athletics', 'STR', 'isk_discipline', 16000396),
    ('History', 'History', 'INT', 'isk_lore', 16000398),
    ('Insight', 'Insight', 'WIS', 'isk_spot', 16000400),
    ('Investigation', 'Investigation', 'INT', 'isk_search', 16000402),
    ('Nature', 'Nature', 'INT', 'isk_aniemp', 16000404),
    ('Religion', 'Religion', 'INT', 'isk_concen', 16000406),
    ('Survival', 'Survival', 'WIS', 'isk_listen', 16000408),
]
ALL_ROWS = NWN_ROWS + [(label, skill) for label, skill, _, _, _ in NEW_ROWS]
# The row the Skills page shows for each 2024 skill (rules::SkillsPageRow).
PAGE_ROW = {
    'Acrobatics': 21, 'Animal Handling': 27, 'Arcana': 7, 'Athletics': 28, 'Deception': 23, 'History': 29,
    'Insight': 30, 'Intimidation': 24, 'Investigation': 31, 'Medicine': 4, 'Nature': 32, 'Perception': 17,
    'Performance': 11, 'Persuasion': 12, 'Religion': 33, 'Sleight of Hand': 13, 'Stealth': 5, 'Survival': 34,
}
EVERY_SKILL = set(PAGE_ROW)

# The 2024 class lists (SRD 5.2; rules::ClassSkillsFor).
CLASS_LISTS = {
    0: {'Animal Handling', 'Athletics', 'Intimidation', 'Nature', 'Perception', 'Survival'},
    1: EVERY_SKILL,
    2: {'History', 'Insight', 'Medicine', 'Persuasion', 'Religion'},
    3: {'Arcana', 'Animal Handling', 'Insight', 'Medicine', 'Nature', 'Perception', 'Religion', 'Survival'},
    4: {'Acrobatics', 'Animal Handling', 'Athletics', 'History', 'Insight', 'Intimidation', 'Persuasion',
        'Perception', 'Survival'},
    5: {'Acrobatics', 'Athletics', 'History', 'Insight', 'Religion', 'Stealth'},
    6: {'Athletics', 'Insight', 'Intimidation', 'Medicine', 'Persuasion', 'Religion'},
    7: {'Animal Handling', 'Athletics', 'Insight', 'Investigation', 'Nature', 'Perception', 'Stealth', 'Survival'},
    8: {'Acrobatics', 'Athletics', 'Deception', 'Insight', 'Intimidation', 'Investigation', 'Perception',
        'Persuasion', 'Sleight of Hand', 'Stealth'},
    9: {'Arcana', 'Deception', 'Insight', 'Intimidation', 'Persuasion', 'Religion'},
    10: {'Arcana', 'History', 'Insight', 'Investigation', 'Medicine', 'Nature', 'Religion'},
}
# NWN's tables (cls_skill_*.2da): the table suffix, the rows a class's table
# leaves out (AllClassesCanUse 0 skills it can't use), and its class skills
# among the rows without a 2024 skill and the tool rows (kept as NWN has them).
NWN_CLASSES = {
    0: ('barb', {0, 11, 19}, {3, 10, 18, 22, 25, 26}),
    1: ('bard', {0}, {1, 3, 10, 18, 19, 20, 22, 25, 26}),
    2: ('cler', {0, 11, 19}, {1, 10, 22, 25, 26}),
    3: ('dru', {11, 19}, {1, 10, 22, 25, 26}),
    4: ('fight', {0, 11, 19}, {1, 3, 10, 22, 25, 26}),
    5: ('monk', {0, 11, 19}, {1, 3, 10, 22, 25, 26}),
    6: ('pal', {0, 11, 19}, {1, 3, 10, 18, 22, 25, 26}),
    7: ('rang', {11, 19}, {1, 3, 10, 15, 22, 25, 26}),
    8: ('rog', {0, 11}, {2, 9, 10, 15, 19, 20, 22, 25, 26}),
    9: ('sorc', {0, 11, 19}, {1, 22, 25, 26}),
    10: ('wiz', {0, 11, 19}, {1, 22, 25, 26}),
}


def write(path, lines):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')


def class_table(nwn_class):
    suffix, left_out, nwn_class_skills = NWN_CLASSES[nwn_class]
    lines = ['2DA V2.0', '', '   SkillLabel      SkillIndex   ClassSkill']
    row = 0
    for index, (label, skill) in enumerate(ALL_ROWS):
        if index in left_out:
            continue
        if skill is None or skill == 'tool':
            class_skill = 1 if index in nwn_class_skills else 0
        else:
            class_skill = 1 if skill in CLASS_LISTS[nwn_class] else 0
        lines.append(f'{row:<4}{label:<16}{index:<13}{class_skill}')
        row += 1
    return f'cls_sk5e_{suffix}', lines


def main():
    rows, files = os.path.join('data', 'rows'), os.path.join('data', 'files')
    for nwn_class in NWN_CLASSES:
        name, lines = class_table(nwn_class)
        write(os.path.join(files, name + '.2da'), lines)
    write(os.path.join(rows, 'classes.if_skills2024.txt'), [
        '# classes.2da - C5 (docs/CHARGEN.md): the base classes\' skill tables with the 2024',
        '# class lists, with the Skills page hooks (generated by tools/data/class_skills.py).',
        'SkillsTable',
    ] + [f'{c} CLS_SK5E_{NWN_CLASSES[c][0].upper()}' for c in NWN_CLASSES])
    write(os.path.join(rows, 'skills.added.txt'), [
        '# skills.2da - C5 (docs/CHARGEN.md): the 2024 skills NWN has no row for, in both',
        '# modes (saves load either way); unusable and off the level-up page unless the',
        '# Skills page hooks serve skills.if_skills2024.new.txt (generated by',
        '# tools/data/class_skills.py; names: data/strings.txt).',
        'Label Name Description Icon Untrained KeyAbility ArmorCheckPenalty AllClassesCanUse Category MaxCR '
        'Constant HostileSkill HideFromLevelUp',
    ] + [f'{28 + i} {label} {strref} {strref + 1} {icon} 1 {ability} 0 0 **** **** '
         f'SKILL_NWN5E_{label.upper()} 0 1' for i, (label, _, ability, icon, strref) in enumerate(NEW_ROWS)])
    write(os.path.join(rows, 'skills.if_skills2024.new.txt'), [
        '# skills.2da - C5: nwn5e\'s rows usable by every class and on the Skills page (with',
        '# the page hooks; generated by tools/data/class_skills.py).',
        'AllClassesCanUse HideFromLevelUp',
    ] + [f'{28 + i} 1 0' for i in range(len(NEW_ROWS))])
    page_rows = set(PAGE_ROW.values())
    write(os.path.join(rows, 'skills.if_skills2024.txt'), [
        '# skills.2da - C5: the Skills page shows one row per 2024 skill (rules::SkillsPageRow);',
        '# the others — rows without a 2024 skill, a skill\'s other rows, the tools — are',
        '# hidden from it, still on the character sheet (with the page hooks; generated by',
        '# tools/data/class_skills.py).',
        'HideFromLevelUp',
    ] + [f'{i} {0 if i in page_rows else 1}' for i in range(len(NWN_ROWS))])
    print(f'{len(NWN_CLASSES)} class tables and the skill rows written')


if __name__ == '__main__':
    main()
