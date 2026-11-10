#include "data/anim_look.h"

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace data {

namespace {

// The spells.2da columns a look may set (the game session's row server takes no others).
const char* const kTableColumns[] = {
    "ConjAnim",       "ConjHeadVisual", "ConjHandVisual", "ConjGrndVisual", "ConjSoundVFX",   "ConjSoundMale",
    "ConjSoundFemale", "CastAnim",      "CastHeadVisual", "CastHandVisual", "CastGrndVisual", "CastSound",
    "Proj",           "ProjModel",      "ProjType",       "ProjSpwnPoint",  "ProjSound",      "ProjOrientation"};

std::string Trim(const std::string& text) {
  const size_t begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return "";
  return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}

bool IsResref(const std::string& text, size_t longest) {
  if (text.empty() || text.size() > longest) return false;
  for (char c : text) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

bool IsTableValue(const std::string& text) {  // one 2DA cell: no spaces, no quotes
  if (text.empty() || text.size() > 32) return false;
  for (char c : text) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '*')) {
      return false;
    }
  }
  return true;
}

bool IsTableColumn(const std::string& column) {
  for (const char* known : kTableColumns) {
    if (column == known) return true;
  }
  return false;
}

bool ParseSeconds(const std::string& text, float* out) {
  char* end = nullptr;
  const float value = strtof(text.c_str(), &end);
  if (text.empty() || *end != '\0' || !(value >= 0.0f && value <= 10.0f)) return false;
  *out = value;
  return true;
}

bool ParseRow(const std::string& text, int* out) {  // a 2DA row number: 0-20000
  char* end = nullptr;
  const long value = strtol(text.c_str(), &end, 10);
  if (text.empty() || *end != '\0' || value < 0 || value > 20000) return false;
  *out = static_cast<int>(value);
  return true;
}

const char* AnchorName(LookAnchor anchor) {
  switch (anchor) {
    case LookAnchor::kRelease: return "release";
    case LookAnchor::kHit: return "hit";
    default: return "cast";
  }
}

std::string Number(float value) {  // at most 3 decimals, trailing zeros trimmed: 1.5, 1, 0.05
  char text[32];
  snprintf(text, sizeof text, "%.3f", value);
  std::string out = text;
  while (!out.empty() && out.back() == '0') out.pop_back();
  if (!out.empty() && out.back() == '.') out.pop_back();
  if (out == "-0") out = "0";
  return out;
}

bool SameSound(const LookSound& a, const LookSound& b) {
  return a.anchor == b.anchor && Number(a.at) == Number(b.at) && a.resref == b.resref;
}

bool SameSounds(const std::vector<LookSound>& a, const std::vector<LookSound>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (!SameSound(a[i], b[i])) return false;
  }
  return true;
}

std::string SoundLine(const LookSound& sound) {
  return std::string("sound = ") + AnchorName(sound.anchor) + " " + Number(sound.at) + " " + sound.resref + "\n";
}

// A parts key (allowed in a section too). Returns false and sets *why when the line is bad; *known = false when the key
// isn't a parts key at all.
bool ReadPartsKey(const std::string& key, const std::string& value, bool* first_sound, LookParts* parts, bool* known,
                  std::string* why) {
  *known = true;
  if (key == "cast_vfx" || key == "missile" || key == "hit" || key == "leap_beam") {
    int row = 0;
    if (!ParseRow(value, &row)) {
      *why = key + " must be a visualeffects row (0 = none)";
      return false;
    }
    (key == "cast_vfx" ? parts->cast_vfx : key == "missile" ? parts->missile : key == "hit" ? parts->hit
                                                                                            : parts->leap_beam) = row;
    return true;
  }
  if (key == "cast_vfx_at" || key == "cast_vfx_for") {
    if (!ParseSeconds(value, key == "cast_vfx_at" ? &parts->cast_vfx_at : &parts->cast_vfx_for)) {
      *why = key + " must be 0-10 s";
      return false;
    }
    return true;
  }
  if (key == "sound") {
    if (*first_sound) {
      parts->sounds.clear();
      *first_sound = false;
    }
    if (value == "none") return true;
    std::istringstream words(value);
    std::string anchor, at, resref, extra;
    words >> anchor >> at >> resref;
    LookSound sound;
    if (anchor == "cast") {
      sound.anchor = LookAnchor::kCast;
    } else if (anchor == "release") {
      sound.anchor = LookAnchor::kRelease;
    } else if (anchor == "hit") {
      sound.anchor = LookAnchor::kHit;
    } else {
      *why = "a sound is 'sound = <cast|release|hit> <seconds> <resref>'";
      return false;
    }
    if (!ParseSeconds(at, &sound.at)) {
      *why = "a sound's time must be 0-10 s";
      return false;
    }
    if (!IsResref(resref, 16) || (words >> extra)) {
      *why = "a sound's resref must be 1-16 of a-z, 0-9 and _";
      return false;
    }
    sound.resref = resref;
    parts->sounds.push_back(sound);
    return true;
  }
  if (key.compare(0, 6, "table.") == 0) {
    const std::string column = key.substr(6);
    if (!IsTableColumn(column)) {
      *why = column + " isn't a presentation column a look may set";
      return false;
    }
    if (!IsTableValue(value)) {
      *why = key + " must be one 2DA value (letters, digits, _ or ****)";
      return false;
    }
    parts->table[column] = value;
    return true;
  }
  *known = false;
  return false;
}

}  // namespace

bool ReadSpellLook(const std::string& text, SpellLook* look, std::string* error) {
  SpellLook l;
  LookParts* parts = &l.parts;
  bool first_sound = false;  // the defaults' sounds accumulate; a section's first sound line clears its copy
  int section_row = 0;
  std::map<int, int> section_lines;
  std::istringstream in(text);
  std::string raw;
  int line_number = 0;
  auto fail = [&](const std::string& why) {  // line 0: the whole file
    if (error) *error = line_number ? "line " + std::to_string(line_number) + ": " + why : why;
    return false;
  };
  while (std::getline(in, raw)) {
    ++line_number;
    const std::string line = Trim(raw.substr(0, raw.find('#')));
    if (line.empty()) continue;
    if (line.front() == '[') {
      int row = 0;
      if (line.back() != ']' || !ParseRow(line.substr(1, line.size() - 2), &row) || row == 0) {
        return fail("a section is '[<spells.2da row>]'");
      }
      if (l.variants.count(row)) return fail("a second [" + std::to_string(row) + "] section");
      l.variants[row] = l.parts;
      parts = &l.variants[row];
      first_sound = true;
      section_row = row;
      section_lines[row] = line_number;
      continue;
    }
    const size_t equals = line.find('=');
    if (equals == std::string::npos) return fail("no '='");
    const std::string key = Trim(line.substr(0, equals));
    const std::string value = Trim(line.substr(equals + 1));
    bool known = false;
    std::string why;
    if (ReadPartsKey(key, value, &first_sound, parts, &known, &why)) continue;
    if (known) return fail(why);
    if (section_row) return fail(key + " can't vary by row (only the parts can)");
    if (key == "name") {
      if (!IsResref(value, 32)) return fail("the name must be 1-32 of a-z, 0-9 and _");
      l.name = value;
    } else if (key == "clip" || key == "clip_base") {
      if (!value.empty() && !IsResref(value, 16)) return fail(key + " must be 1-16 of a-z, 0-9 and _");
      (key == "clip" ? l.clip : l.clip_base) = value;
    } else if (key == "rows") {
      std::istringstream words(value);
      std::string word;
      std::set<int> seen;
      l.rows.clear();
      while (words >> word) {
        int row = 0;
        if (!ParseRow(word, &row) || row == 0 || !seen.insert(row).second) {
          return fail("rows are spells.2da rows, each once");
        }
        l.rows.push_back(row);
      }
    } else if (key == "rule_at" || key == "clip_seconds" || key == "release" || key == "leap_for" ||
               key == "leap_after") {
      float* target = key == "rule_at"        ? &l.rule_at
                      : key == "clip_seconds" ? &l.clip_seconds
                      : key == "release"      ? &l.release
                      : key == "leap_for"     ? &l.leap_for
                                              : &l.leap_after;
      if (!ParseSeconds(value, target)) return fail(key + " must be 0-10 s");
    } else {
      return fail("unknown key '" + key + "'");
    }
  }
  line_number = 0;
  if (l.name.empty()) return fail("a look needs a name");
  if (l.rows.empty()) return fail("a look needs rows (the spells.2da rows it dresses)");
  if (l.release < l.rule_at) {
    return fail("the release (" + Number(l.release) + " s) comes before the rule runs (" + Number(l.rule_at) + " s)");
  }
  for (const auto& section : section_lines) {
    if (std::find(l.rows.begin(), l.rows.end(), section.first) == l.rows.end()) {
      line_number = section.second;
      return fail("a section for row " + std::to_string(section.first) + ", which the look's rows don't list");
    }
  }
  *look = l;
  return true;
}

std::string WriteSpellLook(const SpellLook& look) {
  std::string out = "# nwn5e spell look - key = value\n";
  out += "name = " + look.name + "\n";
  out += "rows =";
  for (int row : look.rows) out += " " + std::to_string(row);
  out += "\n";
  out += "rule_at = " + Number(look.rule_at) + "\n";
  if (!look.clip.empty()) {
    out += "clip = " + look.clip + "\n";
    out += "clip_base = " + look.clip_base + "\n";
    out += "clip_seconds = " + Number(look.clip_seconds) + "\n";
  }
  out += "release = " + Number(look.release) + "\n";
  out += "leap_for = " + Number(look.leap_for) + "\n";
  out += "leap_after = " + Number(look.leap_after) + "\n";
  const LookParts& d = look.parts;
  if (d.cast_vfx != 0) {
    out += "cast_vfx = " + std::to_string(d.cast_vfx) + "\n";
    out += "cast_vfx_at = " + Number(d.cast_vfx_at) + "\n";
    out += "cast_vfx_for = " + Number(d.cast_vfx_for) + "\n";
  }
  out += "missile = " + std::to_string(d.missile) + "\n";
  out += "hit = " + std::to_string(d.hit) + "\n";
  out += "leap_beam = " + std::to_string(d.leap_beam) + "\n";
  for (const LookSound& sound : d.sounds) out += SoundLine(sound);
  for (const auto& cell : d.table) out += "table." + cell.first + " = " + cell.second + "\n";
  for (const auto& variant : look.variants) {
    const LookParts& v = variant.second;
    std::string keys;
    if (v.cast_vfx != d.cast_vfx) keys += "cast_vfx = " + std::to_string(v.cast_vfx) + "\n";
    if (Number(v.cast_vfx_at) != Number(d.cast_vfx_at)) keys += "cast_vfx_at = " + Number(v.cast_vfx_at) + "\n";
    if (Number(v.cast_vfx_for) != Number(d.cast_vfx_for)) keys += "cast_vfx_for = " + Number(v.cast_vfx_for) + "\n";
    if (v.missile != d.missile) keys += "missile = " + std::to_string(v.missile) + "\n";
    if (v.hit != d.hit) keys += "hit = " + std::to_string(v.hit) + "\n";
    if (v.leap_beam != d.leap_beam) keys += "leap_beam = " + std::to_string(v.leap_beam) + "\n";
    if (!SameSounds(v.sounds, d.sounds)) {
      if (v.sounds.empty()) keys += "sound = none\n";
      for (const LookSound& sound : v.sounds) keys += SoundLine(sound);
    }
    for (const auto& cell : v.table) {
      const auto same = d.table.find(cell.first);
      if (same == d.table.end() || same->second != cell.second) {
        keys += "table." + cell.first + " = " + cell.second + "\n";
      }
    }
    if (keys.empty()) continue;  // the same as the defaults: nothing to say
    out += "\n[" + std::to_string(variant.first) + "]\n" + keys;
  }
  return out;
}

const LookParts& LookPartsFor(const SpellLook& look, int row) {
  const auto found = look.variants.find(row);
  return found == look.variants.end() ? look.parts : found->second;
}

float MirvFlightSeconds(float metres) {
  if (!(metres > 0.0f)) return 0.0f;
  return metres / std::max(1.0f, 3.0f * std::log(metres) + 2.0f);
}

LookTiming LookHitTiming(const SpellLook& look, const LookParts& parts, float since_cast, float metres) {
  LookTiming timing;
  timing.late = since_cast > look.release;
  timing.release_in = std::max(0.0f, look.release - since_cast);
  timing.hit_in = timing.release_in + (parts.missile ? MirvFlightSeconds(metres) : 0.0f);
  return timing;
}

namespace {

std::string Seconds(float value) {
  char text[32];
  snprintf(text, sizeof text, "%.2f", std::max(0.0f, value));
  return text;
}

std::string Object(uint32_t id) {
  char text[48];
  snprintf(text, sizeof text, "StringToObject(\"%x\")", id);
  return text;
}

std::string Sound(const char* on, float at, const std::string& resref) {  // `on` null: OBJECT_SELF
  const std::string play = "DelayCommand(" + Seconds(at) + ", PlaySound(\"" + resref + "\"))";
  return on ? std::string("AssignCommand(") + on + ", " + play + ");\n" : play + ";\n";
}

std::string Instant(float at, int row) {
  return "DelayCommand(" + Seconds(at) + ", ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(" +
         std::to_string(row) + "), t));\n";
}

}  // namespace

std::string LookCastScript(const LookParts& parts) {
  std::string code;
  if (parts.cast_vfx) {
    const std::string effect = "EffectVisualEffect(" + std::to_string(parts.cast_vfx) + ")";
    code += "DelayCommand(" + Seconds(parts.cast_vfx_at) + ", ApplyEffectToObject(" +
            (parts.cast_vfx_for > 0.0f ? "DURATION_TYPE_TEMPORARY, " + effect + ", OBJECT_SELF, " +
                                             Seconds(parts.cast_vfx_for)
                                       : "DURATION_TYPE_INSTANT, " + effect + ", OBJECT_SELF") +
            "));\n";
  }
  for (const LookSound& sound : parts.sounds) {
    if (sound.anchor == LookAnchor::kCast) code += Sound(nullptr, sound.at, sound.resref);
  }
  return code;
}

std::string LookHitScript(const LookParts& parts, uint32_t target, const LookTiming& timing) {
  std::string code = "object t = " + Object(target) + ";\n";
  if (parts.missile) code += Instant(timing.release_in, parts.missile);
  if (parts.hit) code += Instant(timing.hit_in, parts.hit);
  for (const LookSound& sound : parts.sounds) {
    if (sound.anchor == LookAnchor::kRelease) code += Sound(nullptr, timing.release_in + sound.at, sound.resref);
    if (sound.anchor == LookAnchor::kHit) code += Sound("t", timing.hit_in + sound.at, sound.resref);
  }
  return code;
}

std::string LookLeapScript(const LookParts& parts, float leap_for, uint32_t from, uint32_t to, float beam_in,
                           float hit_in) {
  std::string code = "object t = " + Object(to) + ";\n";
  if (parts.leap_beam) {
    code += "DelayCommand(" + Seconds(beam_in) + ", ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectBeam(" +
            std::to_string(parts.leap_beam) + ", " + Object(from) + ", BODY_NODE_CHEST), t, " + Seconds(leap_for) +
            "));\n";
  }
  if (parts.hit) code += Instant(hit_in, parts.hit);
  for (const LookSound& sound : parts.sounds) {
    if (sound.anchor == LookAnchor::kHit) code += Sound("t", hit_in + sound.at, sound.resref);
  }
  return code;
}

std::string LookDistanceScript(uint32_t target) {
  return "SetLocalString(OBJECT_SELF, \"n5look_d\", FloatToString(GetDistanceBetween(OBJECT_SELF, " + Object(target) +
         "), 0, 3));\n";
}

}  // namespace data
