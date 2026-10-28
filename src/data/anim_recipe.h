#pragma once

// Animation recipes (owner, 2026-10-25: "Let's try recipe" - "n5test works"):
// a new animation derived from the game's own - a base animation mixed toward
// another by an amount (below 0 it exaggerates away from it), at a speed,
// trimmed. core/anim_packs.cpp builds them at startup from the player's own
// animation libraries (a_ba, a_fa, a_da, a_dfa and their large-build '2's)
// into "animation packs" (n5_ba ...) that the body skeletons' copies point at,
// each pack pointing on at its library - nothing of the game's in nwn5e.
//
// The crouch step (owner, 2026-10-25: a crouch walk - "go, use stealth mode as
// the trigger"), after the mix, speed and trim: the hips drop `crouch` metres
// and each leg is solved again (thigh and shin, the knee in its own bend
// plane) so the ankle stays where the animation had it - same footfalls, same
// stride, knees bending deeper; `lean` tilts the upper body forward - a hip
// hinge: the pelvis tips 35% of it, the torso all of it, the legs solved again
// so the feet stay put - the head tilting 70% of it back to look ahead and the
// arms hanging as the animation swings them, not trailing back with the torso
// (owner, on 15 degrees from the waist only: "the pose still keeps a straight back").
// Only what hangs from the clip's animroot is keyed: a weapon's walk layer
// (walk_swordr ...: animroot torso_g, played over the walk while a weapon is
// held) gets the lean of the torso, head and arms, nothing of the hips or legs. A plain mix toward a
// crouched pose can't do it (measured: walk -> getlowlp 0.3 sinks the planted
// foot 14 cm into the floor, never lifts the other, shortens the stride). The
// legs are solved on the library's bones; the skeletons are uniform scales of
// them (animationscale) except the halflings' shins, ~10% shorter - the same
// mismatch the game's own walk has on them.
//
// The compiled library layout read here (offsets from byte 12, the end of the
// file header): model - root node +0x48, animations +0x78 (pointer, count),
// supermodel name +0xA8; animation - name +8, root node +0x48, length +0x70,
// transtime +0x74, animroot +0x78; node - 0x70 bytes: name +0x20, children
// +0x48 (a list of node pointers, count), controller keys +0x54, values +0x60;
// a key - type, rows, time offset, value offset, columns (position 8: x y z,
// orientation 20: a quaternion x y z w).

#include <array>
#include <map>
#include <string>
#include <vector>

namespace data {

struct AnimKeys {
  std::vector<float> times;
  std::vector<std::array<float, 4>> values;  // positions use the first three
};

// Mocap mode (0.51.0; tools/recipe_bench/public/mocap.js): a recipe carrying its
// own keys - a capture retargeted to each body library by the Animation Recipe
// Bench, one set per library (their bones' rest poses differ). `kind = keys`;
// no base or mix; its own length and transtime. A line per track:
//   keys <library> <bone> o <time x y z w> ...   (orientations)
//   keys <library> <bone> p <time x y z> ...     (positions: the root's)
struct AnimKeyTrack {
  std::string bone;
  bool position = false;
  AnimKeys keys;
};

struct AnimRecipe {
  std::string name;          // the new animation's name (a resref: letters, digits, '_')
  std::string base = "pause1";
  std::string mix;           // the animation it moves toward ("" for none)
  float amount = 0.5f;       // 0 the base, 1 the mix; below 0 exaggerates away from the mix
  float speed = 1.0f;        // playback speed (2 = twice as fast)
  float start = 0.0f;        // the part of the base kept, in its seconds
  float end = 0.0f;          // 0: to its end
  float crouch = 0.0f;       // metres the hips drop, the legs bent to keep the footfalls (0 - 0.6)
  float lean = 0.0f;         // degrees the upper body leans forward, the head looking ahead (-45 - 90)
  // kind = keys (Mocap mode):
  bool keyed = false;
  float length = 0.0f;
  float transtime = 0.25f;
  std::string source;                                         // what it was made from (shown, kept)
  std::map<std::string, std::vector<AnimKeyTrack>> keys;      // library (lower case) -> its tracks
};

std::string WriteAnimRecipe(const AnimRecipe& recipe);
bool ReadAnimRecipe(const std::string& text, AnimRecipe* recipe, std::string* error);

struct AnimNode {
  std::string name;
  int parent = -1;  // index in the list, -1 for the root
  bool has_position = false;
  bool has_orientation = false;
  AnimKeys position;
  AnimKeys orientation;
};

struct AnimClip {
  std::string name;
  float length = 0.0f;
  float transtime = 0.25f;
  std::string animroot;
  std::vector<AnimNode> nodes;  // depth first, the root first
};

struct AnimLibrary {
  std::string name;
  std::string supermodel;
  std::vector<AnimNode> bones;          // the geometry's bone tree (rest pose as the first key)
  std::vector<std::string> clip_names;  // lower case
};

// A compiled library's bone tree and the names of its animations.
bool ReadAnimLibrary(const std::string& bytes, AnimLibrary* library, std::string* error);
// One of its animations by name (any case); false when it hasn't it.
bool ReadAnimClip(const std::string& bytes, const std::string& name, AnimClip* clip);

// The recipe's animation, from its base clip and (when it names one) its mix
// clip; the crouch step needs the library's bones (without them it's skipped).
AnimClip DeriveAnimClip(const AnimRecipe& recipe, const AnimClip& base, const AnimClip* mix,
                        const std::vector<AnimNode>* bones = nullptr);

// A keys recipe's animation for one library: its bones with the recipe's keys
// for that library (bones it doesn't key stay at rest); false when the recipe
// has no keys for that library.
bool KeyedAnimClip(const AnimRecipe& recipe, const std::string& library, const std::vector<AnimNode>& bones,
                   AnimClip* clip);

// The crouch step on a clip, with the library's bones: false when it has
// nothing to do (no crouch or lean, or no rootdummy / torso_g to do it on).
bool ApplyCrouch(AnimClip* clip, const std::vector<AnimNode>& bones, float crouch, float lean_degrees);

// Where each bone is at `t` in the clip (its keys over the bones' rest pose),
// in the model's space: one position per bone, in the bones' order.
std::vector<std::array<float, 3>> AnimWorldPositions(const AnimClip& clip, const std::vector<AnimNode>& bones, float t);

// An ASCII animation pack: the library's bones as dummies and the clips;
// supermodel -> the library.
std::string WriteAnimPack(const std::string& pack, const std::string& library, const std::vector<AnimNode>& bones,
                          const std::vector<AnimClip>& clips);

}  // namespace data
