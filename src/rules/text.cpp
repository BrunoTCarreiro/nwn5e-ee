#include "rules/text.h"

#include <stdint.h>

namespace rules {

namespace {

char Windows1252For(uint32_t code_point) {
  if (code_point < 0x80 || (code_point >= 0xA0 && code_point <= 0xFF)) return static_cast<char>(code_point);
  switch (code_point) {
    case 0x20AC: return static_cast<char>(0x80);  // euro
    case 0x2026: return static_cast<char>(0x85);  // ellipsis
    case 0x2018: return static_cast<char>(0x91);  // quotes
    case 0x2019: return static_cast<char>(0x92);
    case 0x201C: return static_cast<char>(0x93);
    case 0x201D: return static_cast<char>(0x94);
    case 0x2022: return static_cast<char>(0x95);  // bullet
    case 0x2013: return static_cast<char>(0x96);  // en dash
    case 0x2014: return static_cast<char>(0x97);  // em dash
    case 0x2212: return '-';                      // minus sign
    default: return '?';
  }
}

}  // namespace

std::string ToWindows1252(const std::string& utf8) {
  std::string out;
  out.reserve(utf8.size());
  for (size_t i = 0; i < utf8.size();) {
    const unsigned char lead = static_cast<unsigned char>(utf8[i]);
    int length = 1;
    uint32_t code_point = lead;
    if (lead >= 0xF0) {
      length = 4;
      code_point = lead & 0x07;
    } else if (lead >= 0xE0) {
      length = 3;
      code_point = lead & 0x0F;
    } else if (lead >= 0xC0) {
      length = 2;
      code_point = lead & 0x1F;
    } else if (lead >= 0x80) {  // a stray continuation byte
      out += '?';
      ++i;
      continue;
    }
    if (i + length > utf8.size()) {
      out += '?';
      break;
    }
    bool valid = true;
    for (int k = 1; k < length; ++k) {
      const unsigned char next = static_cast<unsigned char>(utf8[i + k]);
      if ((next & 0xC0) != 0x80) valid = false;
      code_point = (code_point << 6) | (next & 0x3F);
    }
    out += valid ? Windows1252For(code_point) : '?';
    i += valid ? length : 1;
  }
  return out;
}

}  // namespace rules
