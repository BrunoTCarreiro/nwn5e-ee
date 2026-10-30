#include "data/game_resources.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace data {

namespace {

// The engine registers "HD0:xp2patch;HD0:xp3;HD0:xp2;HD0:xp1patch;HD0:xp1;
// HD0:patch;HD0:chitin" (0x40EBEA); the first holds the newest data.
const char* const kKeyOrder[] = {"xp2patch", "xp3", "xp2", "xp1patch", "xp1", "patch", "chitin"};

bool ReadFile(const std::wstring& path, std::string* bytes) {
  FILE* file = nullptr;
  if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) return false;
  fseek(file, 0, SEEK_END);
  const long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  bytes->resize(size > 0 ? static_cast<size_t>(size) : 0);
  const size_t read = size > 0 ? fread(&(*bytes)[0], 1, bytes->size(), file) : 0;
  fclose(file);
  return read == bytes->size();
}

uint32_t U32(const std::string& b, size_t at) {
  if (at + 4 > b.size()) return 0;
  return static_cast<uint8_t>(b[at]) | static_cast<uint8_t>(b[at + 1]) << 8 | static_cast<uint8_t>(b[at + 2]) << 16 |
         static_cast<uint32_t>(static_cast<uint8_t>(b[at + 3])) << 24;
}

uint16_t U16(const std::string& b, size_t at) {
  if (at + 2 > b.size()) return 0;
  return static_cast<uint16_t>(static_cast<uint8_t>(b[at]) | static_cast<uint8_t>(b[at + 1]) << 8);
}

std::string Lower(std::string s) {
  for (char& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return s;
}

std::wstring Wide(const std::string& s) { return std::wstring(s.begin(), s.end()); }

std::string EntryName(const std::string& resref, uint16_t type) { return Lower(resref) + "." + std::to_string(type); }

// A loose file's extension for the resource types read here (empty: not looked for loose).
const char* Extension(uint16_t type) {
  switch (type) {
    case kResourceType2DA: return "2da";
    case kResourceTypeMdl: return "mdl";
    case 3: return "tga";
    case 2033: return "dds";
    default: return "";
  }
}

std::vector<std::string> KeyFiles169() {
  std::vector<std::string> keys;
  for (const char* name : kKeyOrder) keys.push_back(std::string(name) + ".key");
  return keys;
}

}  // namespace

GameResources::GameResources(const std::wstring& game_directory)
    : GameResources(game_directory, KeyFiles169(), {game_directory + L"\\override"}) {}

GameResources::GameResources(const std::wstring& game_directory, const std::vector<std::string>& key_files,
                             const std::vector<std::wstring>& override_directories)
    : directory_(game_directory), override_directories_(override_directories) {
  for (const std::string& file : key_files) {
    Key key;
    key.path = file;
    const size_t slash = file.find_last_of("/\\");
    key.name = slash == std::string::npos ? file : file.substr(slash + 1);
    keys_.push_back(key);
  }
}

std::vector<std::string> GameResources::EeKeyFiles() { return {"data/nwn_retail.key", "data/nwn_base.key"}; }

bool GameResources::LoadKey(Key* key) {
  key->loaded = true;
  std::string b;
  if (!ReadFile(directory_ + L"\\" + Wide(key->path), &b) || b.compare(0, 8, "KEY V1  ") != 0) return false;
  const uint32_t bif_count = U32(b, 8), key_count = U32(b, 12), file_table = U32(b, 16), key_table = U32(b, 20);
  for (uint32_t i = 0; i < bif_count; ++i) {
    const size_t at = file_table + i * 12;
    const uint32_t name_offset = U32(b, at + 4);
    const uint16_t name_size = U16(b, at + 8);
    if (name_offset + name_size > b.size()) return false;
    std::string bif = b.substr(name_offset, name_size);
    bif = bif.substr(0, bif.find('\0'));
    key->bifs.push_back(bif);
  }
  for (uint32_t i = 0; i < key_count; ++i) {
    const size_t at = key_table + i * 22;
    if (at + 22 > b.size()) return false;
    std::string resref = b.substr(at, 16);
    resref = resref.substr(0, resref.find('\0'));
    key->entries[EntryName(resref, U16(b, at + 16))] = U32(b, at + 18);
  }
  return true;
}

// Reads only what's needed (a BIF can be hundreds of MB): the header and the
// resource table (kept per BIF), then the one resource.
bool GameResources::ReadFromBif(const std::string& bif, uint32_t resource_index, std::string* bytes) {
  FILE* file = nullptr;
  if (_wfopen_s(&file, (directory_ + L"\\" + Wide(bif)).c_str(), L"rb") != 0 || !file) return false;
  auto table = bif_tables_.find(bif);
  if (table == bif_tables_.end()) {
    std::string header(20, '\0');
    if (fread(&header[0], 1, header.size(), file) != header.size() || header.compare(0, 8, "BIFFV1  ") != 0) {
      fclose(file);
      return false;
    }
    std::string entries(static_cast<size_t>(U32(header, 8)) * 16, '\0');
    if (fseek(file, static_cast<long>(U32(header, 16)), SEEK_SET) != 0 ||
        (!entries.empty() && fread(&entries[0], 1, entries.size(), file) != entries.size())) {
      fclose(file);
      return false;
    }
    table = bif_tables_.emplace(bif, std::move(entries)).first;
  }
  const std::string& entries = table->second;
  bool found = false;
  for (size_t at = 0; at + 16 <= entries.size(); at += 16) {
    if ((U32(entries, at) & 0xFFFFF) != resource_index) continue;
    bytes->assign(U32(entries, at + 8), '\0');
    found = fseek(file, static_cast<long>(U32(entries, at + 4)), SEEK_SET) == 0 &&
            (bytes->empty() || fread(&(*bytes)[0], 1, bytes->size(), file) == bytes->size());
    break;
  }
  fclose(file);
  return found;
}

bool GameResources::Read(const std::string& resref, uint16_t type, std::string* bytes, std::string* source) {
  const std::string extension = Extension(type);
  for (const std::wstring& folder : override_directories_) {
    if (extension.empty()) break;
    if (!ReadFile(folder + L"\\" + Wide(Lower(resref) + "." + extension), bytes)) continue;
    if (source) *source = "override";
    return true;
  }
  const std::string entry = EntryName(resref, type);
  for (Key& key : keys_) {
    if (!key.loaded) LoadKey(&key);
    const auto found = key.entries.find(entry);
    if (found == key.entries.end()) continue;
    const uint32_t bif_index = found->second >> 20;
    if (bif_index >= key.bifs.size()) continue;
    if (!ReadFromBif(key.bifs[bif_index], found->second & 0xFFFFF, bytes)) continue;
    if (source) *source = key.name;
    return true;
  }
  return false;
}

}  // namespace data
