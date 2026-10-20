#pragma once

// Text nwn5e sends to the game. Our sources are UTF-8; NWN 1.69 shows text as
// Windows-1252, so an em dash written as UTF-8 appeared as "â€"" in chat.

#include <string>

namespace rules {

// UTF-8 to Windows-1252: ASCII and Latin-1 as they are, the Windows-1252
// punctuation (dashes, quotes, ellipsis, bullet, euro) to its byte, anything
// else to '?' (U+2212 minus to '-').
std::string ToWindows1252(const std::string& utf8);

}  // namespace rules
