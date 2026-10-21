#include "data/ubjson.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <limits>

namespace data {

namespace {

class Transcoder {
 public:
  Transcoder(const std::string& text, std::string* out) : text_(text), out_(out) {}

  bool Run(std::string* error) {
    SkipSpace();
    if (!Value()) {
      *error = error_ + " at byte " + std::to_string(at_);
      return false;
    }
    SkipSpace();
    if (at_ != text_.size()) {
      *error = "text after the value at byte " + std::to_string(at_);
      return false;
    }
    return true;
  }

 private:
  bool Fail(const char* why) {
    if (error_.empty()) error_ = why;
    return false;
  }

  void SkipSpace() {
    while (at_ < text_.size() && strchr(" \t\r\n", text_[at_])) ++at_;
  }

  bool Literal(const char* word, char marker) {
    const size_t n = strlen(word);
    if (text_.compare(at_, n, word) != 0) return Fail("unknown value");
    at_ += n;
    *out_ += marker;
    return true;
  }

  void BigEndian(uint64_t value, int bytes) {
    for (int i = bytes - 1; i >= 0; --i) *out_ += static_cast<char>((value >> (8 * i)) & 0xff);
  }

  // nlohmann's write_number_with_ubjson_prefix for integers.
  void Integer(int64_t n) {
    if (n >= std::numeric_limits<int8_t>::min() && n <= std::numeric_limits<int8_t>::max()) {
      *out_ += 'i';
      BigEndian(static_cast<uint8_t>(static_cast<int8_t>(n)), 1);
    } else if (n >= 0 && n <= std::numeric_limits<uint8_t>::max()) {
      *out_ += 'U';
      BigEndian(static_cast<uint8_t>(n), 1);
    } else if (n >= std::numeric_limits<int16_t>::min() && n <= std::numeric_limits<int16_t>::max()) {
      *out_ += 'I';
      BigEndian(static_cast<uint16_t>(static_cast<int16_t>(n)), 2);
    } else if (n >= std::numeric_limits<int32_t>::min() && n <= std::numeric_limits<int32_t>::max()) {
      *out_ += 'l';
      BigEndian(static_cast<uint32_t>(static_cast<int32_t>(n)), 4);
    } else {
      *out_ += 'L';
      BigEndian(static_cast<uint64_t>(n), 8);
    }
  }

  bool Number() {
    const size_t start = at_;
    bool fraction = false;
    if (text_[at_] == '-') ++at_;
    while (at_ < text_.size()) {
      const char c = text_[at_];
      if (c >= '0' && c <= '9') {
        ++at_;
      } else if (c == '.' || c == 'e' || c == 'E' || ((c == '+' || c == '-') && fraction)) {
        fraction = true;
        ++at_;
      } else {
        break;
      }
    }
    const std::string number = text_.substr(start, at_ - start);
    if (number.empty() || number == "-") return Fail("bad number");
    char* end = nullptr;
    if (!fraction) {
      const long long value = strtoll(number.c_str(), &end, 10);
      if (*end) return Fail("bad number");
      Integer(value);
      return true;
    }
    const double value = strtod(number.c_str(), &end);
    if (*end) return Fail("bad number");
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    *out_ += 'D';
    BigEndian(bits, 8);
    return true;
  }

  void Utf8(uint32_t code) {
    if (code < 0x80) {
      *out_ += static_cast<char>(code);
    } else if (code < 0x800) {
      *out_ += static_cast<char>(0xc0 | (code >> 6));
      *out_ += static_cast<char>(0x80 | (code & 0x3f));
    } else if (code < 0x10000) {
      *out_ += static_cast<char>(0xe0 | (code >> 12));
      *out_ += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
      *out_ += static_cast<char>(0x80 | (code & 0x3f));
    } else {
      *out_ += static_cast<char>(0xf0 | (code >> 18));
      *out_ += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
      *out_ += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
      *out_ += static_cast<char>(0x80 | (code & 0x3f));
    }
  }

  bool Hex4(uint32_t* code) {
    if (at_ + 4 > text_.size()) return Fail("bad \\u escape");
    *code = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = text_[at_++];
      *code <<= 4;
      if (c >= '0' && c <= '9') *code |= c - '0';
      else if (c >= 'a' && c <= 'f') *code |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') *code |= c - 'A' + 10;
      else return Fail("bad \\u escape");
    }
    return true;
  }

  // A JSON string, decoded, as length + bytes (`marker` 'S' for a value, 0 for a key).
  bool String(bool key) {
    ++at_;  // the opening quote
    std::string* saved = out_;
    std::string decoded;
    out_ = &decoded;
    bool closed = false;
    while (at_ < text_.size()) {
      const char c = text_[at_++];
      if (c == '"') {
        closed = true;
        break;
      }
      if (c != '\\') {
        decoded += c;
        continue;
      }
      if (at_ >= text_.size()) break;
      const char e = text_[at_++];
      switch (e) {
        case '"': decoded += '"'; break;
        case '\\': decoded += '\\'; break;
        case '/': decoded += '/'; break;
        case 'b': decoded += '\b'; break;
        case 'f': decoded += '\f'; break;
        case 'n': decoded += '\n'; break;
        case 'r': decoded += '\r'; break;
        case 't': decoded += '\t'; break;
        case 'u': {
          uint32_t code = 0;
          if (!Hex4(&code)) {
            out_ = saved;
            return false;
          }
          if (code >= 0xd800 && code < 0xdc00 && text_.compare(at_, 2, "\\u") == 0) {
            at_ += 2;
            uint32_t low = 0;
            if (!Hex4(&low)) {
              out_ = saved;
              return false;
            }
            code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
          }
          Utf8(code);
          break;
        }
        default:
          out_ = saved;
          return Fail("bad escape");
      }
    }
    out_ = saved;
    if (!closed) return Fail("unclosed string");
    if (!key) *out_ += 'S';
    Integer(static_cast<int64_t>(decoded.size()));
    *out_ += decoded;
    return true;
  }

  bool Array() {
    ++at_;
    *out_ += '[';
    SkipSpace();
    if (at_ < text_.size() && text_[at_] == ']') {
      ++at_;
      *out_ += ']';
      return true;
    }
    while (true) {
      SkipSpace();
      if (!Value()) return false;
      SkipSpace();
      if (at_ >= text_.size()) return Fail("unclosed array");
      if (text_[at_] == ',') {
        ++at_;
        continue;
      }
      if (text_[at_] == ']') {
        ++at_;
        *out_ += ']';
        return true;
      }
      return Fail("expected , or ]");
    }
  }

  bool Object() {
    ++at_;
    *out_ += '{';
    SkipSpace();
    if (at_ < text_.size() && text_[at_] == '}') {
      ++at_;
      *out_ += '}';
      return true;
    }
    while (true) {
      SkipSpace();
      if (at_ >= text_.size() || text_[at_] != '"') return Fail("expected a key");
      if (!String(true)) return false;
      SkipSpace();
      if (at_ >= text_.size() || text_[at_] != ':') return Fail("expected :");
      ++at_;
      SkipSpace();
      if (!Value()) return false;
      SkipSpace();
      if (at_ >= text_.size()) return Fail("unclosed object");
      if (text_[at_] == ',') {
        ++at_;
        continue;
      }
      if (text_[at_] == '}') {
        ++at_;
        *out_ += '}';
        return true;
      }
      return Fail("expected , or }");
    }
  }

  bool Value() {
    if (at_ >= text_.size()) return Fail("missing value");
    switch (text_[at_]) {
      case '{': return Object();
      case '[': return Array();
      case '"': return String(false);
      case 't': return Literal("true", 'T');
      case 'f': return Literal("false", 'F');
      case 'n': return Literal("null", 'Z');
      default:
        if (text_[at_] == '-' || (text_[at_] >= '0' && text_[at_] <= '9')) return Number();
        return Fail("unexpected character");
    }
  }

  const std::string& text_;
  std::string* out_;
  size_t at_ = 0;
  std::string error_;
};

}  // namespace

bool JsonToUbjson(const std::string& json, std::string* ubjson, std::string* error) {
  ubjson->clear();
  error->clear();
  Transcoder transcoder(json, ubjson);
  if (transcoder.Run(error)) return true;
  ubjson->clear();
  return false;
}

}  // namespace data
