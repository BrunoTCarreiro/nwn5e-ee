#pragma once

// The rules object (CNWRules) loads every rules table in its constructor and
// again in ReloadAll (module loads among its callers). Rules that change
// loaded tables in memory (XP table, class and race fields, feat rows) run
// after each load; served data registers before the first one.

using RulesLoadedHandler = void (*)(void* rules, bool reload);
using RulesLoadingHandler = void (*)();

// After the constructor's loads and after every ReloadAll. If the rules are
// already loaded when this is called, the handler runs at once too.
bool OnRulesLoaded(RulesLoadedHandler handler);

// At the constructor's entry, before any table loads (the resource manager
// exists then).
bool BeforeRulesLoad(RulesLoadingHandler handler);
