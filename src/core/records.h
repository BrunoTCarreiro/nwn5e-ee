#pragma once

#include <initializer_list>
#include <string>

// C6 (docs/CHARGEN.md): nwn5e's per-character records — the ability sync's
// applied deltas, ASI choices, an older !background, the !skills profile,
// class features nwn5e granted, older masteries, the Origin feat a
// background gave. They live in local variables (fast to read), and a player
// character's are also written to its own SQLite database: EE saves that
// database with the character (CNWSCreature::SaveCreature ->
// CNWSObject::SaveSqliteDatabase) and loads it back (LoadCreatureData ->
// LoadSqliteDatabase), exports included — local variables it doesn't save,
// so a campaign chapter's export would lose them.

// Names of records worth keeping (for the first copy of an older character's
// local variables into its database).
void RegisterRecords(std::initializer_list<const char*> names);
void RegisterRecord(const std::string& name);

// Once per character object (a loaded game, an exported character entering a
// new chapter): a player character's records restored from its database
// into local variables, or — the database empty — its older local records
// copied into it. Call before reading records; a no-op afterwards and for
// other creatures.
void EnsureRecords(void* creature);

// A record set: the local variable, and for a player character its database.
void SetRecord(void* creature, const char* name, int value);
