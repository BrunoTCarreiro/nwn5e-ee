#pragma once

// CM8 (which feats exist), faithful: the feats a character can choose are
// the SRD 5.2 feats nwn5e has built — Alert (NWN's Improved Initiative,
// reused under the 2024 name), Ability Score Improvement (progression.h), the
// four Fighting Styles (class_features.h). NWN's other feats can't be chosen
// any more (characters keep the ones they have). The rest of the SRD's 17
// join as they are built.

namespace rules {

constexpr int kFeatAlert = 377;  // NWN's Improved Initiative

// Whether the creation and level-up screens may offer `feat` (faithful).
bool IsChoosableFeat2024(int feat);

}  // namespace rules
