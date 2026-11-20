#pragma once

#include <stdint.h>

// The object under the cursor in the game's view, as the client marks it (#171, 0.99.99: a right-click in the target
// window takes back that target's dart - the owner, 2026-11-20: "right-click A").
//
// The client calls CNWCObject::MouseOver when the cursor comes onto an object (a creature, a placeable, an item) and
// CNWCObject::MouseOff when it leaves. Neither is exported: they're found as the first call in CNWCCreature's own
// MouseOver and MouseOff, its vtable slots 33 and 34 (found by RTTI; the Linux build's symbols name them, slots 34 and
// 35 there with its two destructors), each checked by its first bytes. Missing or changed: logged SKIP, and
// HoveredObject answers 0.

bool InstallClientHover();

// The server id of the object under the cursor, or of the one the cursor left within `left_within_ms` (a right-click
// that cancels a targeting mode can take the cursor off it first); 0 for none.
uint32_t HoveredObject(uint64_t left_within_ms);
