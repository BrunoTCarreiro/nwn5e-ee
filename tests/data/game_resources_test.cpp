#include "check.h"
#include "data/game_resources.h"

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

using namespace data;

namespace {

void Put32(std::string* b, uint32_t v) { for (int i = 0; i < 4; ++i) b->push_back(static_cast<char>((v >> (8 * i)) & 0xFF)); }
void Put16(std::string* b, uint16_t v) { for (int i = 0; i < 2; ++i) b->push_back(static_cast<char>((v >> (8 * i)) & 0xFF)); }

struct Entry {
  std::string resref;
  uint16_t type;
  uint32_t bif;    // which of the key's BIFs
  uint32_t index;  // the resource's index in that BIF
};

// A "KEY V1  " file as the engine writes it: header, file table (12 bytes a BIF), the BIF names,
// the key table (22 bytes an entry: resref[16], type, id = bif << 20 | index).
std::string KeyFile(const std::vector<std::string>& bifs, const std::vector<Entry>& entries) {
  const uint32_t file_table = 64, names = file_table + 12 * static_cast<uint32_t>(bifs.size());
  std::string name_block;
  std::vector<uint32_t> name_at;
  for (const std::string& bif : bifs) {
    name_at.push_back(names + static_cast<uint32_t>(name_block.size()));
    name_block += bif + '\0';
  }
  const uint32_t key_table = names + static_cast<uint32_t>(name_block.size());
  std::string b = "KEY V1  ";
  Put32(&b, static_cast<uint32_t>(bifs.size()));
  Put32(&b, static_cast<uint32_t>(entries.size()));
  Put32(&b, file_table);
  Put32(&b, key_table);
  b.resize(file_table, '\0');
  for (size_t i = 0; i < bifs.size(); ++i) {
    Put32(&b, 0);
    Put32(&b, name_at[i]);
    Put16(&b, static_cast<uint16_t>(bifs[i].size() + 1));
    Put16(&b, 1);
  }
  b += name_block;
  for (const Entry& e : entries) {
    std::string resref = e.resref;
    resref.resize(16, '\0');
    b += resref;
    Put16(&b, e.type);
    Put32(&b, e.bif << 20 | e.index);
  }
  return b;
}

// A "BIFFV1  " file: header, the variable resource table (16 bytes: id, offset, size, type), the data.
std::string BifFile(const std::vector<std::string>& resources) {
  const uint32_t table = 20, data = table + 16 * static_cast<uint32_t>(resources.size());
  std::string b = "BIFFV1  ";
  Put32(&b, static_cast<uint32_t>(resources.size()));
  Put32(&b, 0);
  Put32(&b, table);
  uint32_t at = data;
  for (size_t i = 0; i < resources.size(); ++i) {
    Put32(&b, static_cast<uint32_t>(i));
    Put32(&b, at);
    Put32(&b, static_cast<uint32_t>(resources[i].size()));
    Put32(&b, kResourceTypeMdl);
    at += static_cast<uint32_t>(resources[i].size());
  }
  for (const std::string& r : resources) b += r;
  return b;
}

std::wstring Wide(const std::string& s) { return std::wstring(s.begin(), s.end()); }

// A fresh folder under %TEMP% for one test, with `data\` and `override\` in it.
std::wstring Folder(const char* name) {
  const wchar_t* temp = _wgetenv(L"TEMP");
  std::wstring dir = std::wstring(temp ? temp : L".") + L"\\nwn5e_game_resources_" + Wide(name);
  _wmkdir(dir.c_str());
  _wmkdir((dir + L"\\data").c_str());
  _wmkdir((dir + L"\\override").c_str());
  return dir;
}

void Write(const std::wstring& path, const std::string& bytes) {
  FILE* f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"wb") == 0 && f) {
    fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);
  }
}

}  // namespace

// Breaks caught: the BIF index read as id >> 16; the key list not followed.
TEST(GameResourcesReadsAnEeKey) {
  const std::wstring dir = Folder("ee_key");  // two BIFs, as EE's keys have 60: the body is in the second
  Write(dir + L"\\data\\a.key", KeyFile({"data\\z.bif", "data\\a.bif"}, {{"other", 2002, 1, 0}, {"bodya", 2002, 1, 1}}));
  Write(dir + L"\\data\\z.bif", BifFile({"WRONGBIF", "WRONGBIF"}));
  Write(dir + L"\\data\\a.bif", BifFile({"OTHERBYTES", "MDLBYTES"}));
  GameResources game(dir, {"data/a.key"}, {});
  std::string bytes, source;
  CHECK_EQ(game.Read("bodya", kResourceTypeMdl, &bytes, &source), true);
  CHECK_EQ(bytes == "MDLBYTES", true);
  CHECK_EQ(source == "a.key", true);
  CHECK_EQ(game.Read("BODYA", kResourceTypeMdl, &bytes, &source), true);  // resrefs are case-blind
}

// Breaks caught: the keys searched in reverse.
TEST(GameResourcesFirstKeyWins) {
  const std::wstring dir = Folder("first_key");
  Write(dir + L"\\data\\k1.key", KeyFile({"data\\b1.bif"}, {{"bodya", 2002, 0, 0}}));
  Write(dir + L"\\data\\b1.bif", BifFile({"ONE"}));
  Write(dir + L"\\data\\k2.key", KeyFile({"data\\b2.bif"}, {{"bodya", 2002, 0, 0}}));
  Write(dir + L"\\data\\b2.bif", BifFile({"TWO"}));
  std::string bytes, source;
  GameResources first(dir, {"data/k1.key", "data/k2.key"}, {});
  CHECK_EQ(first.Read("bodya", kResourceTypeMdl, &bytes, &source), true);
  CHECK_EQ(bytes == "ONE", true);
  CHECK_EQ(source == "k1.key", true);
  GameResources second(dir, {"data/k2.key", "data/k1.key"}, {});
  CHECK_EQ(second.Read("bodya", kResourceTypeMdl, &bytes, &source), true);
  CHECK_EQ(bytes == "TWO", true);
}

// Breaks caught: the override folders skipped, or only searched for 2DAs.
TEST(GameResourcesOverrideFolderWins) {
  const std::wstring dir = Folder("override");
  Write(dir + L"\\data\\a.key", KeyFile({"data\\a.bif"}, {{"bodya", 2002, 0, 0}}));
  Write(dir + L"\\data\\a.bif", BifFile({"FROMKEY"}));
  Write(dir + L"\\override\\bodya.mdl", "LOOSE");
  std::string bytes, source;
  GameResources game(dir, {"data/a.key"}, {dir + L"\\override"});
  CHECK_EQ(game.Read("bodya", kResourceTypeMdl, &bytes, &source), true);
  CHECK_EQ(bytes == "LOOSE", true);
  CHECK_EQ(source == "override", true);
  GameResources no_override(dir, {"data/a.key"}, {});
  CHECK_EQ(no_override.Read("bodya", kResourceTypeMdl, &bytes, &source), true);
  CHECK_EQ(bytes == "FROMKEY", true);
}

TEST(GameResourcesMissingIsFalse) {
  const std::wstring dir = Folder("missing");
  Write(dir + L"\\data\\a.key", KeyFile({"data\\a.bif"}, {{"bodya", 2002, 0, 0}}));
  Write(dir + L"\\data\\a.bif", BifFile({"MDL"}));
  std::string bytes, source;
  GameResources game(dir, {"data/a.key"}, {dir + L"\\override"});
  CHECK_EQ(game.Read("nosuch", kResourceTypeMdl, &bytes, &source), false);
  CHECK_EQ(game.Read("bodya", kResourceType2DA, &bytes, &source), false);  // same resref, other type
  GameResources nothing(dir + L"\\nowhere", GameResources::EeKeyFiles(), {});
  CHECK_EQ(nothing.Read("bodya", kResourceTypeMdl, &bytes, &source), false);
}

TEST(GameResourcesEeKeyOrder) {
  const std::vector<std::string> keys = GameResources::EeKeyFiles();
  CHECK_EQ(keys.size(), 2u);
  CHECK_EQ(keys.size() == 2 && keys[0] == "data/nwn_retail.key" && keys[1] == "data/nwn_base.key", true);
  CHECK_EQ(kResourceTypeMdl, 2002);
}
