#include "check.h"
#include "data/twoda.h"
#include "rules/cantrips.h"

#include <fstream>
#include <string>
#include <vector>

// #150 (owner, 2026-11-09: "go with your recommendation for #150"): every cantrip row served (spellcasting faithful)
// casts short - no conjure (ConjTime 0, no ConjAnim), castout (CastAnim out, played as n5_cantrip), for
// rules::kCantripCastMs. NWN's own cantrips (the lists file) have no Innate column: all of them are.
TEST(CantripRowsCastShort) {
  int rows = 0;
  for (const char* name : {"choice", "flame", "new", "utility", "weapon", "lists"}) {
    std::ifstream file(std::string("data/rows/spells.if_cantrips2024.") + name + ".txt", std::ios::binary);
    CHECK_EQ(file.good(), true);
    std::string line;
    std::vector<std::string> head;
    while (std::getline(file, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty() || line[0] == '#') continue;
      const std::vector<std::string> parts = data::Tokenize(line);
      if (head.empty()) {
        head = parts;
        continue;
      }
      auto at = [&](const char* column) -> std::string {
        for (size_t i = 0; i < head.size(); ++i) {
          if (head[i] == column) return i + 1 < parts.size() ? parts[i + 1] : std::string();
        }
        return "(none)";
      };
      if (at("Innate") != "0" && at("Innate") != "(none)") continue;
      CHECK_EQ(at("ConjTime") == "0", true);
      CHECK_EQ(at("ConjAnim") == "****", true);
      CHECK_EQ(at("CastAnim") == "out", true);
      CHECK_EQ(at("CastTime") == std::to_string(rules::kCantripCastMs), true);
      ++rows;
    }
  }
  CHECK_EQ(rows >= 100, true);
}
