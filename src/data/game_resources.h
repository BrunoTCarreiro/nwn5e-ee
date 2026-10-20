#pragma once

// Phase 3 CM0: reads the game's own copy of a resource from the install, the
// way the engine finds it (RE-NOTES "Served data"): the override folder, then
// the key files in the engine's order (xp2patch, xp3, xp2, xp1patch, xp1,
// patch, chitin) and their BIFs. Read-only; no engine code (ports to EE's
// files with another key list).

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

namespace data {

constexpr uint16_t kResourceType2DA = 2017;

class GameResources {
 public:
  explicit GameResources(const std::wstring& game_directory);

  // The resource's bytes; `source` names where they came from ("override",
  // "xp3.key", ...). False when the install has no such resource.
  bool Read(const std::string& resref, uint16_t type, std::string* bytes, std::string* source);

 private:
  struct Key {
    std::string name;                          // "xp3.key"
    std::vector<std::string> bifs;             // relative paths
    std::map<std::string, uint32_t> entries;   // "resref.type" -> resource id
    bool loaded = false;
  };

  bool LoadKey(Key* key);
  bool ReadFromBif(const std::string& bif, uint32_t resource_index, std::string* bytes);

  std::wstring directory_;
  std::vector<Key> keys_;
  // Each BIF's resource table ("data/xp2patch.bif" -> its 16-byte entries).
  std::map<std::string, std::string> bif_tables_;
};

}  // namespace data
