// DH (#172): ModelHasDeathDebris (anim_destruction.h). The model's bytes come through the engine's own resource manager
// (data_serving's ReadGameResource: CExoResMan::Get), so the copy the game plays wins - nwn5e's served folder (LD's
// one-burst copies) over the override folder and the game's keys, a module's haks over all of them; the rule is
// data::HasOwnDeathDebris (src/data/anim_death_debris.h, unit-tested). One answer a model, kept for the session.

#include "anim_destruction.h"

#include <windows.h>

#include <ctype.h>

#include <map>
#include <string>

#include "data/anim_death_debris.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

constexpr size_t kResRefMost = 16;    // a CResRef's characters
constexpr size_t kAnswersMost = 4096;  // models remembered (placeables.2da names 1,304)

SRWLOCK g_lock = SRWLOCK_INIT;

struct Locked {
  explicit Locked(bool exclusive) : exclusive_(exclusive) {
    if (exclusive_) AcquireSRWLockExclusive(&g_lock);
    else AcquireSRWLockShared(&g_lock);
  }
  ~Locked() {
    if (exclusive_) ReleaseSRWLockExclusive(&g_lock);
    else ReleaseSRWLockShared(&g_lock);
  }
  Locked(const Locked&) = delete;
  Locked& operator=(const Locked&) = delete;

 private:
  bool exclusive_;
};

// Model (lower case) -> whether it has its own death debris.
std::map<std::string, bool>& Answers() {
  static std::map<std::string, bool> answers;
  return answers;
}

// A resref as the engine takes one - 1 to 16 letters, digits or '_' - in lower case; "" for anything else (never handed
// to the engine).
std::string ResRef(const std::string& text) {
  if (text.empty() || text.size() > kResRefMost) return std::string();
  std::string out;
  for (char c : text) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (!isalnum(u) && c != '_') return std::string();
    out += static_cast<char>(tolower(u));
  }
  return out;
}

// The engine calls ReadGameResource makes (data_serving.cpp's ReadResource) - CResRef::operator=(const char*),
// CExoResMan::Get(const CResRef&, type), the returned shared_ptr<DataBlock>'s destructor, on *g_pExoResMan - each
// export there and the resource manager made, or no call is made at all.
bool EngineReady(std::string* why) {
  static const bool exports = EngineExport(ee::kResRefAssignChars) && EngineExport(ee::kResManGet) &&
                              EngineExport(ee::kDataBlockPtrDestructor) && EngineExport(ee::kExoResMan);
  if (!exports) {
    *why = "a resource-manager export is missing in this game build";
    return false;
  }
  void* const* resman = reinterpret_cast<void* const*>(EngineExport(ee::kExoResMan));
  if (!resman || !*resman) {
    *why = "no resource manager yet";
    return false;
  }
  return true;
}

// A model's bytes as the game serves them (a supermodel's too); false for a name that is no resref or a model it hasn't.
bool ServedModel(const std::string& name, std::string* bytes) {
  const std::string resref = ResRef(name);
  std::string why;
  return !resref.empty() && ReadGameResource(resref, ee::kResTypeMdl, bytes, &why);
}

bool Decide(const std::string& model_resref) {
  const std::string name = ResRef(model_resref);
  // The key: the resref, or (for a name that is none) the text as given - so that one is logged once too.
  const std::string key = name.empty() ? "?" + model_resref.substr(0, 64) : name;
  {
    Locked read(false);
    const auto found = Answers().find(key);
    if (found != Answers().end()) return found->second;
  }
  bool own = false;
  std::string why;
  if (name.empty()) {
    Log("destruction: \"%s\" is no model resref - no death debris of its own", key.c_str() + 1);
  } else if (!EngineReady(&why)) {
    Log("destruction: %s not read (%s) - no death debris of its own", name.c_str(), why.c_str());
  } else {
    own = data::HasOwnDeathDebris(&ServedModel, name, &why);
    Log("destruction: %s", why.c_str());
  }
  Locked write(true);
  if (Answers().size() < kAnswersMost) Answers().emplace(key, own);
  return own;
}

}  // namespace

bool ModelHasDeathDebris(const std::string& model_resref) {
  try {
    return Decide(model_resref);
  } catch (...) {
    return false;  // (out of memory: no answer kept, none thrown into the game's code)
  }
}
