#include <stdint.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "creature.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "log.h"
#include "records.h"

namespace {

constexpr const char* kVarLoaded = "nwn5e_records";  // 1: restored / migrated this load; 2: no database
constexpr const char* kVarRows = "nwn5e_records_rows";
constexpr const char* kTable = "nwn5e_records";

std::vector<std::string>& Names() {
  static std::vector<std::string> names;
  return names;
}

bool IsPlayerCharacter(void* creature) {
  return creature && Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0;
}

uint32_t IdOf(void* creature) { return Field<uint32_t>(creature, ee::kObjectId); }

// Record names are nwn5e's own constants; checked anyway before going into a script.
bool SafeName(const std::string& name) {
  if (name.empty() || name.size() > 60) return false;
  for (char c : name) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

std::string InsertCode(const std::string& name, int value) {
  return std::string("q = SqlPrepareQueryObject(OBJECT_SELF, \"INSERT OR REPLACE INTO ") + kTable +
         " (k, v) VALUES (@k, @v)\"); SqlBindString(q, \"@k\", \"" + name + "\"); SqlBindInt(q, \"@v\", " +
         std::to_string(value) + "); SqlStep(q);";
}

// A script that didn't run (compile or bridge failure) logged, once per kind.
bool Ran(int result, const char* what) {
  static bool logged[3] = {false, false, false};
  if (result == 0) return true;
  const int kind = what[0] == 'r' ? 0 : what[0] == 'w' ? 1 : 2;
  if (!logged[kind]) {
    logged[kind] = true;
    Log("records: the %s script failed (RunScriptChunk %d)", what, result);
  }
  return false;
}

std::string RestoreCode() {
  return std::string("sqlquery q = SqlPrepareQueryObject(OBJECT_SELF, \"CREATE TABLE IF NOT EXISTS ") + kTable +
         " (k TEXT PRIMARY KEY, v INTEGER)\"); SqlStep(q); q = SqlPrepareQueryObject(OBJECT_SELF, \"SELECT k, v FROM " +
         kTable + "\"); int n = 0; while (SqlStep(q)) { SetLocalInt(OBJECT_SELF, SqlGetString(q, 0), "
         "SqlGetInt(q, 1)); n++; } SetLocalInt(OBJECT_SELF, \"" + kVarRows + "\", n); SetLocalInt(OBJECT_SELF, \"" +
         kVarLoaded + "\", 1);";
}

}  // namespace

void RegisterRecords(std::initializer_list<const char*> names) {
  for (const char* name : names) Names().push_back(name);
}

void RegisterRecord(const std::string& name) { Names().push_back(name); }

void EnsureRecords(void* creature) {
  if (!IsPlayerCharacter(creature) || engine::GetLocalInt(creature, kVarLoaded) != 0) return;
  if (!engine::AreaOf(creature)) return;  // still loading: the script would have no object to work on
  const uint32_t id = IdOf(creature);
  Ran(RunScriptChunk(RestoreCode().c_str(), id), "restore");
  if (engine::GetLocalInt(creature, kVarLoaded) != 1) {
    engine::SetLocalInt(creature, kVarLoaded, 2);  // no database: local variables only (not retried this load)
    Log("records: 0x%08X the character's database didn't answer - records stay in local variables", id);
    return;
  }
  const int rows = engine::GetLocalInt(creature, kVarRows);
  if (rows > 0) {
    Log("records: 0x%08X %d record(s) restored from the character's database", id, rows);
    return;
  }
  // An older character (or a new one): its records so far, into the database.
  std::string copy = "sqlquery q;";
  int copied = 0;
  for (const std::string& name : Names()) {
    const int value = engine::GetLocalInt(creature, name.c_str());
    if (value == 0 || !SafeName(name)) continue;
    copy += " " + InsertCode(name, value);
    ++copied;
  }
  if (copied > 0) {
    Ran(RunScriptChunk(copy.c_str(), id), "copy");
    Log("records: 0x%08X %d older record(s) copied from local variables into the character's database", id, copied);
  }
}

void SetRecord(void* creature, const char* name, int value) {
  if (!creature) return;
  EnsureRecords(creature);  // first, so a restore can't overwrite this value with an older one
  engine::SetLocalInt(creature, name, value);
  if (!IsPlayerCharacter(creature) || engine::GetLocalInt(creature, kVarLoaded) != 1 || !SafeName(name)) return;
  Ran(RunScriptChunk(("sqlquery " + InsertCode(name, value)).c_str(), IdOf(creature)), "write");
}
