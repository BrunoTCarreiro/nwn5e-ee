#pragma once

// JSON text to UBJSON, the binary form EE's client reads NUI layouts and bind
// values in (nlohmann's from_ubjson, strict). nwn5e builds its layouts as JSON
// text (nui.cpp); its client windows (client_ui.cpp) hand the engine UBJSON
// through the engine's own message writers. Written as nlohmann's to_ubjson
// writes it: the smallest integer marker (i, U, I, l, L), D for any number
// with a fraction or exponent, S + length for strings, object keys as length
// + bytes, no counts or types on containers; numbers big-endian.

#include <string>

namespace data {

// false (and *error set) when `json` isn't one complete JSON value.
bool JsonToUbjson(const std::string& json, std::string* ubjson, std::string* error);

}  // namespace data
