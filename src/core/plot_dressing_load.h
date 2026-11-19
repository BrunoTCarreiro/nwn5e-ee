#pragma once

#include "rules/plot_dressing.h"

// #172: whether a placeable just loaded (light_sources.cpp's LoadPlaceable hook) is Plot dressing with nothing
// attached that loads breakable - its conversation and event scripts read from its GFF struct; `facts` holds the rest.
bool PlotDressingFreed(void* gff, void* gff_struct, rules::PlaceableFacts facts);
