#include "data/script_pieces.h"

#include <algorithm>

namespace data {

namespace {

std::string Literal(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

}  // namespace

std::string ScriptStringBuild(const std::string& var, const std::string& text, size_t piece) {
  piece = std::max<size_t>(piece, 1);
  std::string out = "string " + var + " = " + Literal(text.substr(0, piece)) + ";";
  for (size_t at = piece; at < text.size(); at += piece) out += " " + var + " += " + Literal(text.substr(at, piece)) + ";";
  return out;
}

}  // namespace data
