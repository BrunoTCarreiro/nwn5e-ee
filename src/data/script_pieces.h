#pragma once

#include <stddef.h>

#include <string>

namespace data {

// #176 (the owner, 2026-11-21: "doing "!vfx" crashed the game"): a compiled script holds a string constant's length in
// 16 bits - the VFX browser's 1,136 labels as one 35 KB literal crashed the game building it (CExoString::
// InitFromCharArray under CVirtualMachine::ExecuteCode). A long text for a script chunk is built in a variable instead:
// "string <var> = "<piece>"; <var> += "<piece>"; ..." - each piece at most `piece` characters of the text (escaped
// after the split, so an escape is never cut). An empty text: "string <var> = "";".
constexpr size_t kScriptPiece = 4000;
std::string ScriptStringBuild(const std::string& var, const std::string& text, size_t piece = kScriptPiece);

}  // namespace data
