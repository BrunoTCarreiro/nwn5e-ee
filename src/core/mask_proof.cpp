// "!maskproof [off]" (#184's research spike; the owner, 2026-11-23: "is there a way to make a "mask" effect? an effect
// that makes particles from other effects disappear", then "run the research spike now"), a test command: [debug]
// test_commands=1 and mask_proof=1 (off by default, so an install changes nothing until it's tried).
// - What it proves: that an emitter draws through a shader of nwn5e's own (EE 1.87.8193.35's "emitters with custom
//   shaders": its texture is the material n5_pmask, data\files\n5_pmask.mtr, which pairs the game's vsparticle with
//   fsn5pmask - the game's own fsparticle with nwn5e's mask, made at build time by tools/data/particle_mask.js); that the
//   per-player scriptable uniforms (NWScript SetShaderUniformVec, the same build) reach it; and that the mask - a
//   fragment dropped when the line from the effect's point of origin to it passes through an obstacle's box
//   (rules/particle_mask.h) - lines up with the area's own boxes, while another effect's particles stay.
// - What it does: the nearest placeable box within 8 m whose box doesn't hold the origin (the cover's own list:
//   object_cover.cpp - cover_objects=1, the 2024 rules); the origin 1.2 m over the speaker's feet (the ball's emitter);
//   the uniforms set for the speaker (scriptableVec1..3: that box); a ball of flames round the origin for 20 s through
//   the masked shader (visualeffects row 1848, n5_pmaskc), and the control - a plain flame column on the ground 1 m past
//   the box's far side, on the line from the origin through its centre, drawn with the game's own particle shader (1849,
//   n5_pmaskt). The flames behind the box should vanish, the plain one stay.
// - "!maskproof off": scriptableVec1 back to zero (every uniform's default): the mask off, the ball whole again - the
//   shader then draws exactly as the game's own.
// The detail in nwn5e.log ("mask proof: ..."): the box, the origin, the uniforms, the script's result.

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "log.h"
#include "object_cover.h"
#include "player_text.h"
#include "rules/cover_objects.h"
#include "rules/particle_mask.h"

namespace {

constexpr int kFireVfx = 1848;   // data/rows/visualeffects.maskproof.txt: the ball, through the masked shader
constexpr int kPlainVfx = 1849;  // the control, through the game's own
constexpr float kOriginHeight = 1.2f;  // n5_pmaskc's emitter over its spot
constexpr float kNearMetres = 8.0f;
constexpr float kPlainPast = 1.0f;  // the control's distance past the box's far side

std::string Trimmed(std::string s) {
  while (!s.empty() && s.front() == ' ') s.erase(s.begin());
  while (!s.empty() && s.back() == ' ') s.pop_back();
  return s;
}

std::string OneDecimal(float v) {
  char text[32];
  sprintf_s(text, "%.1f", v);
  return text;
}

// The NWScript that sets scriptableVec1.. to `uniforms` for OBJECT_SELF.
std::string UniformCode(const std::vector<rules::MaskVec>& uniforms) {
  std::string code;
  for (size_t i = 0; i < uniforms.size(); ++i) {
    char line[200];
    sprintf_s(line, "SetShaderUniformVec(OBJECT_SELF, SHADER_UNIFORM_%d, %.4f, %.4f, %.4f, %.4f);",
              static_cast<int>(i) + 1, uniforms[i].x, uniforms[i].y, uniforms[i].z, uniforms[i].w);
    code += line;
  }
  return code;
}

}  // namespace

std::vector<std::string> MaskProofCommand(void* creature, const std::string& args) {
  if (!g_config.debug_mask_proof) return {PlayerText("test_commands.maskproof_disabled")};
  const std::string arg = Trimmed(args);
  const uint32_t self = Field<uint32_t>(creature, ee::kObjectId);
  if (arg == "off") {
    const int ran = RunScriptChunk("SetShaderUniformVec(OBJECT_SELF, SHADER_UNIFORM_1, 0.0, 0.0, 0.0, 0.0);", self);
    Log("mask proof: 0x%08X off - scriptableVec1 zero (script %d)", self, ran);
    return {PlayerText("test_commands.maskproof_cleared")};
  }
  if (!arg.empty()) return {PlayerText("test_commands.maskproof_usage")};
  if (!ServedFileExists(L"fsn5pmask.shd")) {
    Log("mask proof: no served fsn5pmask.shd - build_derived.js's \"particle mask\" line says why");
    return {PlayerText("test_commands.maskproof_no_shader")};
  }
  const float px = Field<float>(creature, ee::kObjectPositionX), py = Field<float>(creature, ee::kObjectPositionY),
              pz = Field<float>(creature, ee::kObjectPositionZ);
  const float origin[3] = {px, py, pz + kOriginHeight};
  const std::vector<NearObjectBox> nearby = ObjectBoxesNear(creature, kNearMetres);
  const NearObjectBox* pick = nullptr;
  for (const NearObjectBox& candidate : nearby) {
    if (!rules::PointInBox(origin, candidate.box)) {
      pick = &candidate;
      break;
    }
  }
  if (!pick) {
    Log("mask proof: 0x%08X - no box within %.0f m (%zu listed near, cover objects %s)", self, kNearMetres, nearby.size(),
        CoverObjectsOn() ? "on" : "off");
    return {PlayerText("test_commands.maskproof_no_box", {{"metres", OneDecimal(kNearMetres)}})};
  }
  const rules::ObjectBox& box = pick->box;
  const std::vector<rules::MaskVec> uniforms = rules::PackParticleMask(origin, {box});
  // The control: on the ground, on the line from the origin through the box's centre, 1 m past its far side.
  float dx = box.centre[0] - origin[0], dy = box.centre[1] - origin[1];
  const float length = hypotf(dx, dy);
  if (length < 0.01f) {
    dx = 1.0f;
    dy = 0.0f;
  } else {
    dx /= length;
    dy /= length;
  }
  const float reach = fabsf(dx * box.ax + dy * box.ay) * box.half[0] + fabsf(dx * box.bx + dy * box.by) * box.half[1];
  const float plain[2] = {box.centre[0] + dx * (reach + kPlainPast), box.centre[1] + dy * (reach + kPlainPast)};
  char effects[512];
  sprintf_s(effects,
            "object a = GetArea(OBJECT_SELF);"
            "ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(%d), Location(a, Vector(%.3f, %.3f, %.3f), 0.0));"
            "ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(%d), Location(a, Vector(%.3f, %.3f, %.3f), 0.0));",
            kFireVfx, px, py, pz, kPlainVfx, plain[0], plain[1], pz);
  const int ran = RunScriptChunk((UniformCode(uniforms) + effects).c_str(), self);
  Log("mask proof: 0x%08X - %s (0x%08X) %.2f m away: centre (%.2f, %.2f, %.2f), half (%.2f, %.2f, %.2f), +X (%.3f, %.3f); "
      "origin (%.2f, %.2f, %.2f); plain flame at (%.2f, %.2f, %.2f); %zu uniforms, scriptableVec1 w %.0f (script %d)",
      self, pick->name.c_str(), pick->id, pick->metres, box.centre[0], box.centre[1], box.centre[2], box.half[0],
      box.half[1], box.half[2], box.ax, box.ay, origin[0], origin[1], origin[2], plain[0], plain[1], pz,
      uniforms.size(), uniforms[0].w, ran);
  return {PlayerText("test_commands.maskproof_on", {{"name", pick->name}, {"metres", OneDecimal(pick->metres)}})};
}
