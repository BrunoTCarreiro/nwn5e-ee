#include <string>

#include "check.h"
#include "data/ubjson.h"

using namespace data;

namespace {
std::string Bytes(std::initializer_list<int> list) {
  std::string out;
  for (int b : list) out += static_cast<char>(b);
  return out;
}
}  // namespace

// Breaks caught (nwn5e's client windows: EE reads layouts and binds as
// UBJSON, nlohmann's from_ubjson, strict): a key written with the string
// marker, little-endian numbers, an integer sent as a float (or the reverse),
// a length that doesn't fit its marker, escapes left in a string, trailing
// bytes accepted, a broken text accepted.
TEST(UbjsonMatchesNlohmann) {
  std::string out, error;
  CHECK_EQ(JsonToUbjson("\"ab\"", &out, &error), true);
  CHECK_EQ(out == Bytes({0x53, 0x69, 0x02, 'a', 'b'}), true);
  CHECK_EQ(JsonToUbjson("{\"x\":1}", &out, &error), true);
  CHECK_EQ(out == Bytes({0x7b, 0x69, 0x01, 'x', 0x69, 0x01, 0x7d}), true);
  CHECK_EQ(JsonToUbjson("[null,true,false]", &out, &error), true);
  CHECK_EQ(out == Bytes({0x5b, 0x5a, 0x54, 0x46, 0x5d}), true);
  CHECK_EQ(JsonToUbjson("200", &out, &error), true);
  CHECK_EQ(out == Bytes({0x55, 200}), true);
  CHECK_EQ(JsonToUbjson("-200", &out, &error), true);
  CHECK_EQ(out == Bytes({0x49, 0xff, 0x38}), true);  // int16, big-endian
  CHECK_EQ(JsonToUbjson("70000", &out, &error), true);
  CHECK_EQ(out == Bytes({0x6c, 0x00, 0x01, 0x11, 0x70}), true);  // int32
  CHECK_EQ(JsonToUbjson("1.5", &out, &error), true);
  CHECK_EQ(out == Bytes({0x44, 0x3f, 0xf8, 0, 0, 0, 0, 0, 0}), true);  // float64
  CHECK_EQ(JsonToUbjson("40.0", &out, &error), true);
  CHECK_EQ(out[0] == 'D', true);  // still a float
  CHECK_EQ(JsonToUbjson("\"a\\\"b\\\\c\\n\\u00e9\"", &out, &error), true);
  CHECK_EQ(out == Bytes({0x53, 0x69, 0x08, 'a', '"', 'b', '\\', 'c', '\n', 0xc3, 0xa9}), true);  // é = 2 bytes
  const std::string long_text(300, 'z');
  CHECK_EQ(JsonToUbjson("\"" + long_text + "\"", &out, &error), true);
  CHECK_EQ(out.substr(0, 4) == Bytes({0x53, 0x49, 0x01, 0x2c}), true);  // length 300 as int16
  CHECK_EQ(JsonToUbjson(" { \"a\" : [ 1 , { } ] } ", &out, &error), true);
  CHECK_EQ(out == Bytes({0x7b, 0x69, 0x01, 'a', 0x5b, 0x69, 0x01, 0x7b, 0x7d, 0x5d, 0x7d}), true);
  CHECK_EQ(JsonToUbjson("{\"a\":1} x", &out, &error), false);
  CHECK_EQ(JsonToUbjson("{\"a\":}", &out, &error), false);
  CHECK_EQ(JsonToUbjson("[1,]", &out, &error), false);
  CHECK_EQ(JsonToUbjson("\"open", &out, &error), false);
  CHECK_EQ(error.empty(), false);
}
