#include "rules/test_profile.h"

#include <ctype.h>
#include <stdlib.h>

#include <sstream>

namespace rules {

namespace {

std::string Trim(const std::string& text) {
  const size_t first = text.find_first_not_of(" \t\r");
  if (first == std::string::npos) return "";
  return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

std::string Lower(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

std::vector<std::string> SplitCommas(const std::string& text) {
  std::vector<std::string> parts;
  std::string part;
  std::istringstream in(text);
  while (std::getline(in, part, ',')) {
    part = Trim(part);
    if (!part.empty()) parts.push_back(part);
  }
  return parts;
}

int RaceFromName(const std::string& name) {
  static const char* const kRaces[] = {"dwarf", "elf", "gnome", "halfling", "half-elf", "half-orc", "human"};
  for (int i = 0; i < 7; ++i) {
    if (name == kRaces[i]) return i;
  }
  if (name == "halfelf") return 4;
  if (name == "halforc" || name == "orc") return 5;
  return -1;
}

int ClassFromName(const std::string& name) {
  static const char* const kClasses[] = {"barbarian", "bard",    "cleric", "druid",    "fighter", "monk",
                                         "paladin",   "ranger",  "rogue",  "sorcerer", "wizard"};
  for (int i = 0; i < 11; ++i) {
    if (name == kClasses[i]) return i;
  }
  return -1;
}

bool ParseItem(const std::string& text, TestItem* item) {
  std::istringstream in(text);
  std::string word;
  if (!(in >> word)) return false;
  item->blueprint = Lower(word);
  if (item->blueprint.size() > 16) return false;
  for (char c : item->blueprint) {
    if (!isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
  }
  while (in >> word) {
    word = Lower(word);
    if (word == "equip") {
      item->equip = true;
    } else if (word == "offhand") {
      item->equip = item->offhand = true;
    } else if (word.size() > 1 && word[0] == 'x' && isdigit(static_cast<unsigned char>(word[1]))) {
      item->stack = atoi(word.c_str() + 1);
      if (item->stack < 1 || item->stack > 99) return false;
    } else {
      return false;
    }
  }
  return true;
}

// "<switch>=<value>", a switch a profile may set and a value in its range.
bool ParseSwitch(const std::string& text, TestSwitch* test_switch) {
  const size_t equals = text.find('=');
  if (equals == std::string::npos) return false;
  test_switch->name = Lower(Trim(text.substr(0, equals)));
  const std::string number = Trim(text.substr(equals + 1));
  if (number.empty() || number.size() > 6) return false;
  for (char c : number) {
    if (!isdigit(static_cast<unsigned char>(c))) return false;
  }
  test_switch->value = atoi(number.c_str());
  int low = 0, high = 0;
  return TestSwitchRange(test_switch->name, &low, &high) && test_switch->value >= low && test_switch->value <= high;
}

}  // namespace

bool IsTestProfileName(const std::string& name) {
  if (name.empty() || name.size() > 32) return false;
  for (char c : name) {
    if (!isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') return false;
  }
  return true;
}

bool TestProfileBuildsCharacter(const TestProfile& profile) {
  return profile.swap_class >= 0 || profile.nwn_class >= 0 || !profile.levels.empty();
}

bool TestSwitchRange(const std::string& name, int* low, int* high) {
  struct Range {
    const char* name;
    int low, high;
  };
  static const Range kRanges[] = {{"attack_trace", 0, 1},
                                  {"attacks_per_round", 0, 6},
                                  {"creature_hp_multiplier", 0, 100},
                                  {"creature_min_hp", 0, 30000},
                                  {"rest_clock", 0, 1}};
  for (const Range& range : kRanges) {
    if (name == range.name) {
      *low = range.low;
      *high = range.high;
      return true;
    }
  }
  return false;
}

TestProfile ParseTestProfile(const std::string& text) {
  TestProfile profile;
  std::istringstream in(text);
  std::string line;
  int number = 0;
  while (std::getline(in, line)) {
    ++number;
    const size_t comment = line.find('#');
    if (comment != std::string::npos) line = line.substr(0, comment);
    line = Trim(line);
    if (line.empty()) continue;
    const size_t equals = line.find('=');
    if (equals == std::string::npos) {
      profile.errors.push_back("line " + std::to_string(number) + ": no '='");
      continue;
    }
    const std::string key = Lower(Trim(line.substr(0, equals)));
    const std::string value = Trim(line.substr(equals + 1));
    const std::string where = "line " + std::to_string(number) + " (" + key + "): ";
    if (key == "name") {
      profile.name = value;
    } else if (key == "species") {
      profile.race = RaceFromName(Lower(value));
      if (profile.race < 0) profile.errors.push_back(where + "unknown species \"" + value + "\"");
    } else if (key == "class") {
      profile.nwn_class = ClassFromName(Lower(value));
      if (profile.nwn_class < 0) profile.errors.push_back(where + "unknown class \"" + value + "\"");
    } else if (key == "swap_class") {
      profile.swap_class = ClassFromName(Lower(value));
      if (profile.swap_class < 0) profile.errors.push_back(where + "unknown class \"" + value + "\"");
    } else if (key == "xp") {
      profile.xp = atoi(value.c_str());
      if (profile.xp < 1 || profile.xp > 10000000) profile.errors.push_back(where + "xp is 1..10000000");
    } else if (key == "level") {
      profile.level = atoi(value.c_str());
      if (profile.level < 1 || profile.level > 20) profile.errors.push_back(where + "a level is 1..20");
    } else if (key == "abilities") {
      std::istringstream scores(value);
      int count = 0, score = 0;
      while (count < 6 && scores >> score) profile.abilities[count++] = score;
      profile.has_abilities = count == 6;
      for (int i = 0; i < count; ++i) {
        if (profile.abilities[i] < 3 || profile.abilities[i] > 30) profile.has_abilities = false;
      }
      if (!profile.has_abilities) profile.errors.push_back(where + "six scores 3..30, Str Dex Con Int Wis Cha");
    } else if (key == "feats") {
      for (const std::string& feat : SplitCommas(value)) profile.feats.push_back(feat);
    } else if (key == "items") {
      for (const std::string& part : SplitCommas(value)) {
        TestItem item;
        if (ParseItem(part, &item)) {
          profile.items.push_back(item);
        } else {
          profile.errors.push_back(where + "\"" + part + "\" is not \"<blueprint> [xN] [equip] [offhand]\"");
        }
      }
    } else if (key == "levels") {
      for (const std::string& part : SplitCommas(value)) {
        std::istringstream words(part);
        std::string class_name, extra;
        TestLevels levels;
        if (words >> class_name >> levels.level && !(words >> extra)) levels.nwn_class = ClassFromName(Lower(class_name));
        if (levels.nwn_class >= 0 && levels.level >= 1 && levels.level <= 20) {
          profile.levels.push_back(levels);
        } else {
          profile.errors.push_back(where + "\"" + part + "\" is not \"<class> <level 1..20>\"");
        }
      }
    } else if (key == "switches") {
      for (const std::string& part : SplitCommas(value)) {
        TestSwitch test_switch;
        if (ParseSwitch(part, &test_switch)) {
          profile.switches.push_back(test_switch);
        } else {
          profile.errors.push_back(where + "\"" + part +
                                   "\": a profile sets attack_trace, attacks_per_round, creature_hp_multiplier, "
                                   "creature_min_hp or rest_clock, each with a value in its range");
        }
      }
    } else if (key == "note") {
      if (!value.empty()) profile.notes.push_back(value);
    } else {
      profile.errors.push_back(where + "unknown key");
    }
  }
  if (profile.level > 0 && profile.nwn_class < 0) profile.errors.push_back("a level needs a class");
  return profile;
}

}  // namespace rules
