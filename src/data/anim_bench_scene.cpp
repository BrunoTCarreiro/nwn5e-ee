#include "data/anim_bench_scene.h"

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace data {

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kLeapMetres = 9.144f;  // 30 feet
const Spot kHuddle[] = {{0.0f, 0.0f}, {0.9f, 0.8f},  {-0.8f, 0.9f}, {0.7f, -0.9f},
                        {-0.9f, -0.7f}, {1.6f, 0.0f}, {-1.6f, 0.0f}, {0.0f, 1.6f}};

bool IsResref(const std::string& text) {
  if (text.empty() || text.size() > 16) return false;
  for (char c : text) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

bool ParseInt(const std::string& text, int low, int high, int* out) {
  char* end = nullptr;
  const long value = strtol(text.c_str(), &end, 10);
  if (text.empty() || *end != '\0' || value < low || value > high) return false;
  *out = static_cast<int>(value);
  return true;
}

std::string Fixed(float value) {
  char text[32];
  snprintf(text, sizeof text, "%.2f", value);
  return text;
}

}  // namespace

bool FormationFromName(const std::string& name, Formation* kind) {
  for (Formation f : {Formation::kOne, Formation::kLine, Formation::kCluster, Formation::kRing, Formation::kAround}) {
    if (name == FormationName(f)) {
      *kind = f;
      return true;
    }
  }
  return false;
}

const char* FormationName(Formation kind) {
  switch (kind) {
    case Formation::kLine: return "line";
    case Formation::kCluster: return "cluster";
    case Formation::kRing: return "ring";
    case Formation::kAround: return "around";
    default: return "one";
  }
}

std::vector<Spot> FormationSpots(Formation kind, int count, float distance) {
  const int n = kind == Formation::kOne ? 1 : std::min(8, std::max(1, count));
  const float d = std::min(30.0f, std::max(1.0f, distance));
  std::vector<Spot> spots;
  for (int i = 0; i < n; ++i) {
    const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(n);
    Spot s;
    switch (kind) {
      case Formation::kLine:
        s = {d, (static_cast<float>(i) - static_cast<float>(n - 1) / 2.0f) * 1.5f};
        break;
      case Formation::kCluster:
        s = {d + kHuddle[i].ahead, kHuddle[i].right};
        break;
      case Formation::kRing:
        s = {d + 2.0f * std::cos(angle), 2.0f * std::sin(angle)};
        break;
      case Formation::kAround:
        s = {d * std::cos(angle), d * std::sin(angle)};
        break;
      default:
        s = {d, 0.0f};
        break;
    }
    spots.push_back(s);
  }
  return spots;
}

std::vector<int> LeapOrder(const std::vector<Spot>& spots, int target, int leaps) {
  std::vector<int> order;
  if (target < 0 || target >= static_cast<int>(spots.size())) return order;
  order.push_back(target);
  for (int k = 0; k < leaps; ++k) {
    const Spot& from = spots[order.back()];
    int best = -1;
    float best_metres = kLeapMetres;
    for (int j = 0; j < static_cast<int>(spots.size()); ++j) {
      if (std::find(order.begin(), order.end(), j) != order.end()) continue;
      const float metres = std::hypot(spots[j].ahead - from.ahead, spots[j].right - from.right);
      if (metres <= best_metres && (best < 0 || metres < best_metres)) {
        best = j;
        best_metres = metres;
      }
    }
    if (best < 0) break;
    order.push_back(best);
  }
  return order;
}

bool ReadBenchRequest(const std::string& first_line, BenchRequest* request, std::string* error) {
  auto fail = [&](const std::string& why) {
    if (error) *error = why;
    return false;
  };
  std::istringstream words(first_line);
  std::string request_word, equals, verb, serial;
  words >> request_word >> equals >> verb >> serial;
  BenchRequest r;
  if (request_word != "request" || equals != "=" || (verb != "play" && verb != "stop")) {
    return fail("the first line is 'request = play|stop <serial> ...'");
  }
  if (!ParseInt(serial, 0, 2000000000, &r.serial)) return fail("a request needs its serial number");
  r.play = verb == "play";
  std::string pair;
  while (words >> pair) {
    const size_t at = pair.find('=');
    const std::string key = pair.substr(0, at), value = at == std::string::npos ? "" : pair.substr(at + 1);
    bool ok = true;
    if (key == "row") {
      ok = ParseInt(value, 1, 20000, &r.row);
    } else if (key == "formation") {
      ok = FormationFromName(value, &r.formation);
    } else if (key == "count") {
      ok = ParseInt(value, 1, 8, &r.count);
    } else if (key == "distance") {
      char* end = nullptr;
      r.distance = strtof(value.c_str(), &end);
      ok = !value.empty() && *end == '\0' && r.distance >= 1.0f && r.distance <= 30.0f;
    } else if (key == "target") {
      ok = ParseInt(value, 0, 7, &r.target);
    } else if (key == "enemy") {
      ok = IsResref(value);
      r.enemy = value;
    } else if (key == "leaps") {
      ok = ParseInt(value, 0, 8, &r.leaps);
    } else {
      return fail("unknown request key '" + key + "'");
    }
    if (!ok) return fail("a bad " + key + ": '" + value + "'");
  }
  if (r.play) {
    if (r.row == 0) return fail("a play request needs row=<spells.2da row>");
    const int spots = static_cast<int>(FormationSpots(r.formation, r.count, r.distance).size());
    if (r.target >= spots) return fail("target " + std::to_string(r.target) + " is past the formation's creatures");
  }
  *request = r;
  return true;
}

std::string BenchSpawnScript(const std::vector<Spot>& spots, const std::string& enemy) {
  std::string code =
      "object pc = OBJECT_SELF;\nvector p = GetPosition(pc);\nfloat f = GetFacing(pc);\nstring ids = \"\";\n"
      "location l;\nobject o;\n";
  for (const Spot& s : spots) {
    code += "l = Location(GetArea(pc), p + AngleToVector(f) * " + Fixed(s.ahead) + " + AngleToVector(f - 90.0) * " +
            Fixed(s.right) + ", f + 180.0);\n";
    code += "o = CreateObject(OBJECT_TYPE_CREATURE, \"" + enemy + "\", l, FALSE, \"n5sb_target\");\n";
    code += "ChangeToStandardFaction(o, STANDARD_FACTION_COMMONER);\nSetPlotFlag(o, TRUE);\n";
    code += "ids += ObjectToString(o) + \";\";\n";
  }
  code += "SetLocalString(OBJECT_SELF, \"n5sb_spawned\", ids);\n";
  return code;
}

std::string BenchClearScript() {
  return "int i = 0;\nobject o = GetObjectByTag(\"n5sb_target\", i);\n"
         "while (GetIsObjectValid(o)) {\n  DestroyObject(o);\n  i++;\n  o = GetObjectByTag(\"n5sb_target\", i);\n}\n";
}

std::string BenchCastScript(uint32_t target, int row) {
  char text[64];
  snprintf(text, sizeof text, "object t = StringToObject(\"%x\");\n", target);
  return std::string(text) + "ClearAllActions();\nSetFacingPoint(GetPosition(t));\nActionCastFakeSpellAtObject(" +
         std::to_string(row) + ", t);\n";
}

}  // namespace data
