#include "data/vfx_area.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace data {

const char* const kVfxAreaDraftHeader =
    "# nwn5e area draft (the VFX Bench's area mode, #161) - key = value; slot <k> = model copies lifetime-s edge-0-1 "
    "[low-detail model]";

namespace {

constexpr const char* kBlank = "****";

float Clamp(float v, float lo, float hi) { return std::isfinite(v) ? std::max(lo, std::min(hi, v)) : lo; }

std::string Num(float v) {
  char text[32];
  std::snprintf(text, sizeof(text), "%g", std::round(v * 10000.0f) / 10000.0f);
  return text;
}

std::string OrBlank(const std::string& s) { return s.empty() ? kBlank : s; }

float Radius(const VfxArea& a) { return Clamp(a.radius, kVfxAreaMinRadius, kVfxAreaMaxSize); }
float Side(float v) { return Clamp(v, kVfxAreaMinSide, kVfxAreaMaxSize); }
int Count(int n) { return std::max(1, std::min(kVfxAreaMaxCount, n)); }
float Lifetime(float s) { return Clamp(s, 0.0f, kVfxAreaMaxLifetime); }
float Edge(float e) { return Clamp(e, 0.0f, 1.0f); }

bool IsName(const std::string& s) {
  if (s.empty() || s.size() > 16) return false;
  for (char c : s)
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
  return true;
}

bool ToFloat(const std::string& s, float* out) {
  if (s.empty()) return false;
  char* end = nullptr;
  const float v = std::strtof(s.c_str(), &end);
  if (!end || *end != '\0' || !std::isfinite(v)) return false;
  *out = v;
  return true;
}

bool ToInt(const std::string& s, int* out) {
  if (s.empty()) return false;
  char* end = nullptr;
  const long v = std::strtol(s.c_str(), &end, 10);
  if (!end || *end != '\0') return false;
  *out = static_cast<int>(std::max(-100000L, std::min(100000L, v)));
  return true;
}

std::string Trim(const std::string& s) {
  const size_t a = s.find_first_not_of(" \t");
  if (a == std::string::npos) return "";
  const size_t b = s.find_last_not_of(" \t");
  return s.substr(a, b - a + 1);
}

std::vector<std::string> Words(const std::string& s) {
  std::istringstream in(s);
  std::vector<std::string> out;
  for (std::string w; in >> w;) out.push_back(w);
  return out;
}

}  // namespace

std::vector<std::pair<std::string, std::string>> VfxAreaRowColumns(const VfxArea& area) {
  std::vector<std::pair<std::string, std::string>> out;
  const bool rect = area.shape == 'R';
  out.emplace_back("SHAPE", rect ? "R" : "C");
  out.emplace_back("RADIUS", rect ? kBlank : Num(Radius(area)));
  out.emplace_back("WIDTH", rect ? Num(Side(area.width)) : kBlank);
  out.emplace_back("LENGTH", rect ? Num(Side(area.length)) : kBlank);
  out.emplace_back("OrientWithGround", area.orient_ground ? "1" : "0");
  out.emplace_back("DurationVFX", OrBlank(area.duration_effect));
  for (int i = 0; i < 3; ++i) {
    const VfxAreaSlot& s = area.slots[i];
    const std::string k = "0" + std::to_string(i + 1);
    const bool on = !s.model.empty();
    out.emplace_back("MODEL" + k, on ? s.model : kBlank);
    out.emplace_back("NUMACT" + k, on ? std::to_string(Count(s.count)) : kBlank);
    out.emplace_back("DURATION" + k, on ? std::to_string(static_cast<int>(std::lround(Lifetime(s.lifetime) * 1000.0f))) : kBlank);
    out.emplace_back("EDGEWGHT" + k, on ? Num(Edge(s.edge)) : kBlank);
    out.emplace_back("MODELMIN" + k, on ? OrBlank(s.low_model) : kBlank);
  }
  out.emplace_back("SoundImpact", OrBlank(area.sound_impact));
  out.emplace_back("SoundDuration", OrBlank(area.sound_duration));
  out.emplace_back("SoundCessation", OrBlank(area.sound_cessation));
  out.emplace_back("SoundOneShot", OrBlank(area.sound_oneshot));
  out.emplace_back("SoundOneShotPercentage", area.sound_oneshot.empty() ? kBlank : Num(Edge(area.oneshot_chance)));
  return out;
}

std::string WriteVfxAreaDraft(const VfxArea& area) {
  std::string out = std::string(kVfxAreaDraftHeader) + "\n";
  out += "area = " + area.name + "\n";
  if (area.shape == 'R') {
    out += "shape = R\nwidth = " + Num(Side(area.width)) + "\nlength = " + Num(Side(area.length)) + "\n";
  } else {
    out += "radius = " + Num(Radius(area)) + "\n";
  }
  for (int i = 0; i < 3; ++i) {
    const VfxAreaSlot& s = area.slots[i];
    if (s.model.empty()) continue;
    out += "slot " + std::to_string(i + 1) + " = " + s.model + " " + std::to_string(Count(s.count)) + " " + Num(Lifetime(s.lifetime)) +
           " " + Num(Edge(s.edge)) + (s.low_model.empty() ? "" : " " + s.low_model) + "\n";
  }
  if (!area.duration_effect.empty()) out += "duration_effect = " + area.duration_effect + "\n";
  if (!area.sound_impact.empty()) out += "sound_impact = " + area.sound_impact + "\n";
  if (!area.sound_duration.empty()) out += "sound_duration = " + area.sound_duration + "\n";
  if (!area.sound_cessation.empty()) out += "sound_cessation = " + area.sound_cessation + "\n";
  if (!area.sound_oneshot.empty()) out += "sound_oneshot = " + area.sound_oneshot + " " + Num(Edge(area.oneshot_chance)) + "\n";
  if (area.orient_ground) out += "orient_ground = 1\n";
  return out;
}

bool ReadVfxAreaDraft(const std::string& text, VfxArea* area, std::string* err) {
  VfxArea a;
  std::istringstream in(text);
  std::string raw;
  int n = 0;
  auto fail = [&](const std::string& why) {
    if (err) *err = "line " + std::to_string(n) + ": " + why;
    return false;
  };
  while (std::getline(in, raw)) {
    ++n;
    if (!raw.empty() && raw.back() == '\r') raw.pop_back();
    const std::string line = Trim(raw);
    if (line.empty() || line[0] == '#') continue;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) return fail("no '=' in \"" + line + "\"");
    const std::string key = Trim(line.substr(0, eq)), value = Trim(line.substr(eq + 1));
    float f = 0.0f;
    if (key == "area") {
      if (!IsName(value)) return fail("the area's name \"" + value + "\" isn't 1-16 of A-Z a-z 0-9 _");
      a.name = value;
    } else if (key == "shape") {
      if (value != "C" && value != "R") return fail("shape is C (a circle) or R (a rectangle), not \"" + value + "\"");
      a.shape = value[0];
    } else if (key == "radius" || key == "width" || key == "length") {
      if (!ToFloat(value, &f)) return fail(key + " \"" + value + "\" isn't a number");
      if (key == "radius") a.radius = Clamp(f, kVfxAreaMinRadius, kVfxAreaMaxSize);
      else if (key == "width") a.width = Side(f);
      else a.length = Side(f);
    } else if (key.rfind("slot", 0) == 0) {
      int k = 0;
      if (!ToInt(Trim(key.substr(4)), &k) || k < 1 || k > 3) return fail("\"" + key + "\": the slots are 1 to 3");
      const std::vector<std::string> w = Words(value);
      if (w.size() < 4 || w.size() > 5) return fail("a slot is: model copies lifetime edge [low-detail model]");
      VfxAreaSlot s;
      int count = 0;
      float life = 0.0f, edge = 0.0f;
      if (!IsName(w[0])) return fail("the model \"" + w[0] + "\" isn't 1-16 of A-Z a-z 0-9 _");
      if (!ToInt(w[1], &count) || !ToFloat(w[2], &life) || !ToFloat(w[3], &edge)) return fail("a slot's copies, lifetime and edge are numbers");
      if (w.size() == 5 && !IsName(w[4])) return fail("the low-detail model \"" + w[4] + "\" isn't 1-16 of A-Z a-z 0-9 _");
      s.model = w[0];
      s.count = Count(count);
      s.lifetime = Lifetime(life);
      s.edge = Edge(edge);
      s.low_model = w.size() == 5 ? w[4] : "";
      a.slots[k - 1] = s;
    } else if (key == "duration_effect") {
      int row = 0;
      if (!ToInt(value, &row) || row < 0) return fail("duration_effect is a visualeffects row number, not \"" + value + "\"");
      a.duration_effect = std::to_string(row);
    } else if (key == "sound_impact" || key == "sound_duration" || key == "sound_cessation") {
      if (!IsName(value)) return fail("the sound \"" + value + "\" isn't 1-16 of A-Z a-z 0-9 _");
      (key == "sound_impact" ? a.sound_impact : key == "sound_duration" ? a.sound_duration : a.sound_cessation) = value;
    } else if (key == "sound_oneshot") {
      const std::vector<std::string> w = Words(value);
      if (w.empty() || w.size() > 2 || !IsName(w[0])) return fail("sound_oneshot is: sound [chance 0-1]");
      if (w.size() == 2 && !ToFloat(w[1], &f)) return fail("the one-shot chance \"" + w[1] + "\" isn't a number");
      a.sound_oneshot = w[0];
      a.oneshot_chance = w.size() == 2 ? Edge(f) : 0.0f;
    } else if (key == "orient_ground") {
      if (value != "0" && value != "1") return fail("orient_ground is 0 or 1");
      a.orient_ground = value == "1";
    } else {
      return fail("unknown key \"" + key + "\"");
    }
  }
  if (a.name.empty()) {
    if (err) *err = "no area name (area = <name>)";
    return false;
  }
  *area = a;
  if (err) err->clear();
  return true;
}

}  // namespace data
