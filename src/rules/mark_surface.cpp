#include "rules/mark_surface.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>

#include <algorithm>

namespace rules {

Vec3f Add(const Vec3f& a, const Vec3f& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3f Sub(const Vec3f& a, const Vec3f& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3f Scale(const Vec3f& v, float s) { return {v.x * s, v.y * s, v.z * s}; }
float Dot(const Vec3f& a, const Vec3f& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3f Cross(const Vec3f& a, const Vec3f& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float Length(const Vec3f& v) { return sqrtf(Dot(v, v)); }
Vec3f Normalized(const Vec3f& v) {
  const float length = Length(v);
  return length > 1e-9f ? Scale(v, 1.0f / length) : Vec3f{};
}

Vec3f Rotate(const Quatf& q, const Vec3f& v) {
  const Vec3f axis{q.x, q.y, q.z};
  const Vec3f t = Scale(Cross(axis, v), 2.0f);
  return Add(Add(v, Scale(t, q.w)), Cross(axis, t));
}

Quatf Conjugate(const Quatf& q) { return {q.w, -q.x, -q.y, -q.z}; }

Quatf TurnFromAxes(const Vec3f& x, const Vec3f& y, const Vec3f& z) {
  // The turn's matrix has the axes as its columns.
  const float m00 = x.x, m01 = y.x, m02 = z.x;
  const float m10 = x.y, m11 = y.y, m12 = z.y;
  const float m20 = x.z, m21 = y.z, m22 = z.z;
  const float trace = m00 + m11 + m22;
  Quatf q;
  if (trace > 0.0f) {
    const float s = sqrtf(trace + 1.0f) * 2.0f;
    q = {0.25f * s, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s};
  } else if (m00 > m11 && m00 > m22) {
    const float s = sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
    q = {(m21 - m12) / s, 0.25f * s, (m01 + m10) / s, (m02 + m20) / s};
  } else if (m11 > m22) {
    const float s = sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
    q = {(m02 - m20) / s, (m01 + m10) / s, 0.25f * s, (m12 + m21) / s};
  } else {
    const float s = sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
    q = {(m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, 0.25f * s};
  }
  const float length = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
  if (length < 1e-9f) return Quatf{};
  return {q.w / length, q.x / length, q.y / length, q.z / length};
}

float MarkEyeHeight(int creature_size) {
  switch (creature_size) {
    case 1:
      return 0.6f;  // tiny
    case 2:
      return 1.0f;  // small
    case 4:
      return 2.4f;  // large
    case 5:
      return 3.6f;  // huge
    default:
      return 1.6f;  // medium
  }
}

Vec3f MarkAimPoint(const Vec3f& eye, const Vec3f& box_min, const Vec3f& box_max) {
  Vec3f aim = Scale(Add(box_min, box_max), 0.5f);
  const float height = box_max.z - box_min.z;
  if (height <= 0.0f) return aim;
  if (eye.z > box_max.z) {
    aim.z = box_max.z - 0.05f * height;
  } else {
    aim.z = std::clamp(eye.z, box_min.z + 0.1f * height, box_max.z - 0.1f * height);
  }
  return aim;
}

std::vector<Vec3f> MarkAimPoints(const Vec3f& eye, const Vec3f& box_min, const Vec3f& box_max) {
  const Vec3f first = MarkAimPoint(eye, box_min, box_max);
  const float across_x = 0.35f * (box_max.x - box_min.x);
  const float across_y = 0.35f * (box_max.y - box_min.y);
  const float height = box_max.z - box_min.z;
  const float higher = std::min(first.z + 0.25f * height, box_max.z);
  const float lower = std::max(first.z - 0.25f * height, box_min.z);
  return {first,
          {first.x + across_x, first.y, first.z},
          {first.x - across_x, first.y, first.z},
          {first.x, first.y + across_y, first.z},
          {first.x, first.y - across_y, first.z},
          {first.x, first.y, higher},
          {first.x, first.y, lower}};
}

Vec3f MarkRayEnd(const Vec3f& eye, const Vec3f& aim, const Vec3f& box_min, const Vec3f& box_max) {
  const Vec3f direction = Normalized(Sub(aim, eye));
  return Add(aim, Scale(direction, Length(Sub(box_max, box_min))));
}

void MarkRayOffsets(const Vec3f& direction, Vec3f* u, Vec3f* v) {
  const Vec3f d = Normalized(direction);
  Vec3f across = Cross(d, Vec3f{0, 0, 1});
  if (Length(across) < 1e-3f) across = Cross(d, Vec3f{1, 0, 0});  // looking straight up or down
  across = Normalized(across);
  if (u) *u = Scale(across, kMarkNormalOffset);
  if (v) *v = Scale(Normalized(Cross(across, d)), kMarkNormalOffset);
}

Vec3f MarkSurfaceNormal(const Vec3f& direction, const Vec3f& p0, const std::optional<Vec3f>& p1,
                        const std::optional<Vec3f>& p2) {
  const Vec3f d = Normalized(direction);
  const Vec3f back = Scale(d, -1.0f);
  if (!p1 || !p2) return back;
  const Vec3f a = Sub(*p1, p0);
  const Vec3f b = Sub(*p2, p0);
  if (fabsf(Dot(a, d)) > kMarkSameSurface || fabsf(Dot(b, d)) > kMarkSameSurface) return back;
  const Vec3f n = Cross(a, b);
  if (Length(n) <= 1e-3f * Length(a) * Length(b)) return back;  // in a line
  const Vec3f unit = Normalized(n);
  return Dot(unit, d) > 0.0f ? Scale(unit, -1.0f) : unit;
}

Quatf MarkTurnOn(const Vec3f& normal, const Vec3f& direction) {
  const Vec3f n = Normalized(normal);
  Vec3f up_from{0, 0, 1};
  if (fabsf(n.z) > kMarkFlatSurface) {
    up_from = Normalized(Vec3f{direction.x, direction.y, 0});
    if (Length(up_from) == 0.0f) up_from = {0, 1, 0};  // looking straight down
  }
  Vec3f up = Normalized(Sub(up_from, Scale(n, Dot(up_from, n))));
  if (Length(up) == 0.0f) up = Normalized(Cross(Vec3f{1, 0, 0}, n));
  return TurnFromAxes(Cross(n, up), n, up);
}

MarkFix MarkFixInPart(const std::string& part, const Vec3f& hit, const Vec3f& normal, const Vec3f& direction,
                      const Vec3f& part_position, const Quatf& part_turn) {
  const Vec3f n = Normalized(normal);
  const float length =
      sqrtf(part_turn.w * part_turn.w + part_turn.x * part_turn.x + part_turn.y * part_turn.y + part_turn.z * part_turn.z);
  const Quatf unit = length > 1e-9f ? Quatf{part_turn.w / length, part_turn.x / length, part_turn.y / length,
                                            part_turn.z / length}
                                    : Quatf{};
  const Quatf back = Conjugate(unit);
  const Quatf world = MarkTurnOn(n, direction);
  MarkFix fix;
  fix.part = part;
  fix.position = Rotate(back, Sub(Add(hit, Scale(n, kMarkSurfaceGap)), part_position));
  fix.turn = TurnFromAxes(Rotate(back, Rotate(world, Vec3f{1, 0, 0})), Rotate(back, Rotate(world, Vec3f{0, 1, 0})),
                          Rotate(back, Rotate(world, Vec3f{0, 0, 1})));
  return fix;
}

Vec3f MarkClickEnd(const MarkRay& ray) {
  return Add(ray.to, Scale(Normalized(Sub(ray.to, ray.from)), kMarkClickReach));
}

namespace {

bool PlainPartName(const std::string& name) {
  if (name.empty() || name.size() > 63) return false;
  for (char c : name) {
    if (c == ' ' || c == '|' || c == '\t' || c == '\r' || c == '\n') return false;
  }
  return true;
}

}  // namespace

std::string MarkRecordText(const MarkRecord& record) {
  char text[256];
  int used = snprintf(text, sizeof(text), "e %.3f %.3f %.3f", record.eye.x, record.eye.y, record.eye.z);
  if (record.click && used > 0) {
    const MarkRay& c = *record.click;
    const int more = snprintf(text + used, sizeof(text) - used, "|c %.3f %.3f %.3f %.3f %.3f %.3f", c.from.x, c.from.y,
                              c.from.z, c.to.x, c.to.y, c.to.z);
    if (more > 0) used += more;
  }
  if (record.fix && PlainPartName(record.fix->part) && used > 0 && used < static_cast<int>(sizeof(text))) {
    const MarkFix& f = *record.fix;
    snprintf(text + used, sizeof(text) - used, "|f %s %.4f %.4f %.4f %.5f %.5f %.5f %.5f", f.part.c_str(), f.position.x,
             f.position.y, f.position.z, f.turn.w, f.turn.x, f.turn.y, f.turn.z);
  }
  return text;
}

bool ParseMarkRecord(const std::string& text, MarkRecord* record) {
  if (!record) return false;
  Vec3f eye;
  int read = 0;
  if (sscanf_s(text.c_str(), "e %f %f %f%n", &eye.x, &eye.y, &eye.z, &read) < 3 || read <= 0) return false;
  record->eye = eye;
  record->click.reset();
  record->fix.reset();
  const char* rest = text.c_str() + read;
  if (*rest == '\0') return true;
  if (*rest != '|') return false;
  MarkRay click;
  int click_read = 0;
  if (sscanf_s(rest, "|c %f %f %f %f %f %f%n", &click.from.x, &click.from.y, &click.from.z, &click.to.x, &click.to.y,
               &click.to.z, &click_read) == 6 &&
      click_read > 0) {
    record->click = click;
    rest += click_read;
  }
  char part[64] = {};
  MarkFix fix;
  if (sscanf_s(rest, "|f %63s %f %f %f %f %f %f %f", part, static_cast<unsigned>(sizeof(part)), &fix.position.x,
               &fix.position.y, &fix.position.z, &fix.turn.w, &fix.turn.x, &fix.turn.y, &fix.turn.z) == 8) {
    fix.part = part;
    record->fix = fix;
  }
  return true;  // a part that doesn't read: no fix (the mark is placed again)
}

MarkClickKind ClassifyMarkClick(const Vec3f& cursor, const Vec3f& normal, const std::optional<float>& ground) {
  if (MarkStandsUpright(normal)) return MarkClickKind::kWall;
  if (!ground || cursor.z - *ground > kMarkAboveGround) return MarkClickKind::kTop;
  return MarkClickKind::kFloor;
}

Vec3f MarkCastPointBefore(const Vec3f& cursor, const Vec3f& camera, float feet_z) {
  const Vec3f toward = Normalized(Vec3f{cursor.x - camera.x, cursor.y - camera.y, 0});
  return {cursor.x - toward.x * kMarkWallStandOff, cursor.y - toward.y * kMarkWallStandOff, feet_z};
}

bool MarkStandsUpright(const Vec3f& normal) { return fabsf(Normalized(normal).z) <= kMarkFlatSurface; }

bool MarkCursorOnCaster(const Vec3f& cursor, const Vec3f& feet) {
  const float across = sqrtf((cursor.x - feet.x) * (cursor.x - feet.x) + (cursor.y - feet.y) * (cursor.y - feet.y));
  const float up = cursor.z - feet.z;
  return across <= kMarkBodyReach && up >= -0.2f && up <= kMarkBodyHeight;
}

bool MarkPassesThrough(const std::string& part) {
  return part.size() == 3 && tolower(static_cast<unsigned char>(part[0])) == 's' &&
         tolower(static_cast<unsigned char>(part[1])) == 'a' && tolower(static_cast<unsigned char>(part[2])) == 'm';
}

float MarkFacingOf(const Vec3f& normal) {
  float degrees = atan2f(normal.y, normal.x) * 180.0f / 3.14159265f;
  if (degrees < 0.0f) degrees += 360.0f;
  return degrees;
}

}  // namespace rules
