#include "data/effect_nodes.h"

#include <ctype.h>
#include <string.h>

namespace data {

namespace {

// visualeffects.2da's model columns: an effect's impact or duration model at each attach point, and its cessation
// model.
const char* const kModelColumns[] = {
    "Imp_HeadCon_Node", "Imp_Impact_Node", "Imp_Root_S_Node", "Imp_Root_M_Node", "Imp_Root_L_Node", "Imp_Root_H_Node",
    "Ces_HeadCon_Node", "Ces_Impact_Node", "Ces_Root_S_Node", "Ces_Root_M_Node", "Ces_Root_L_Node", "Ces_Root_H_Node"};

int ColumnOf(const TwoDA& table, const char* name) {
  for (size_t c = 0; c < table.columns.size(); ++c) {
    if (_stricmp(table.columns[c].c_str(), name) == 0) return static_cast<int>(c);
  }
  return -1;
}

std::string Lower(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

// A cell's value, "" for "****" or a column the row lacks.
std::string Cell(const std::vector<std::string>& row, int column) {
  if (column < 0 || static_cast<size_t>(column) >= row.size() || row[column] == kEmpty) return std::string();
  return row[column];
}

}  // namespace

bool ValidNodeName(const std::string& name) {
  if (name.empty() || name.size() > 31) return false;
  for (char c : name) {
    if (!isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
  }
  return true;
}

EffectNodes EffectNodesFrom(const TwoDA& table) {
  EffectNodes out;
  const int node_column = ColumnOf(table, kEffectNodeColumn);
  if (node_column < 0) return out;
  const int upright_column = ColumnOf(table, kEffectNodeUprightColumn);
  std::vector<int> model_columns;
  for (const char* name : kModelColumns) {
    const int column = ColumnOf(table, name);
    if (column >= 0) model_columns.push_back(column);
  }
  // The models of rows without a node: they stay where the game puts them.
  std::map<std::string, size_t> elsewhere;  // model -> the first such row
  for (size_t r = 0; r < table.rows.size(); ++r) {
    if (!Cell(table.rows[r], node_column).empty()) continue;
    for (int column : model_columns) {
      const std::string model = Lower(Cell(table.rows[r], column));
      if (!model.empty()) elsewhere.emplace(model, r);
    }
  }
  std::map<std::string, size_t> given_by;  // model -> the row that gave it its node
  for (size_t r = 0; r < table.rows.size(); ++r) {
    const std::string given = Cell(table.rows[r], node_column);
    if (given.empty()) continue;
    const std::string row = std::to_string(r);
    if (!ValidNodeName(given)) {
      out.refused.push_back("row " + row + ": \"" + given + "\" is not a node name (1-31 letters, digits or _)");
      continue;
    }
    EffectNode where;
    where.node = Lower(given);
    where.upright = Cell(table.rows[r], upright_column) == "1";
    for (int column : model_columns) {
      const std::string model = Lower(Cell(table.rows[r], column));
      if (model.empty()) continue;
      const auto shared = elsewhere.find(model);
      if (shared != elsewhere.end()) {
        out.refused.push_back("row " + row + ": " + model + " is row " + std::to_string(shared->second) +
                              "'s too, which names no node - it stays where the game puts it (give the row a model "
                              "of its own)");
        continue;
      }
      const auto known = out.models.find(model);
      if (known != out.models.end()) {
        if (known->second.node != where.node || known->second.upright != where.upright) {
          out.refused.push_back("row " + row + ": " + model + " already goes to " + known->second.node + " (row " +
                                std::to_string(given_by[model]) + ")");
        }
        continue;
      }
      out.models[model] = where;
      given_by[model] = r;
    }
  }
  return out;
}

}  // namespace data
