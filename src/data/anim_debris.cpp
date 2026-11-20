#include "data/anim_debris.h"

namespace data {

namespace {

const char* const kMaterialCodes[kDebrisMaterials] = {"wood", "stone", "metal", "cloth", "glass"};

bool Valid(int material, int size) {
  return material >= 0 && material < kDebrisMaterials && size >= 0 && size < kDebrisSizes;
}

}  // namespace

int DebrisRow(int material, int size) {
  return Valid(material, size) ? kDebrisFirstRow + material * kDebrisSizes + size : -1;
}

std::string DebrisMaterialCode(int material) {
  return material >= 0 && material < kDebrisMaterials ? kMaterialCodes[material] : "";
}

std::string DebrisModel(int material, int size) {
  if (!Valid(material, size)) return "";
  return std::string("n5_db_") + kMaterialCodes[material] + (size == 0 ? "_s" : "_l");
}

std::string LightDeathModel() { return "n5_db_light"; }

}  // namespace data
