#pragma once
#include "render_ir/scene.h"
#include <bit>

namespace dfv::runtime {
// 只用于不可变绑定与碰撞缓存；逐字段编码，不读取结构体填充字节。
struct GeometryKey {
  uint64_t value=14695981039346656037ull;
  void add(uint64_t v) {value^=v;value*=1099511628211ull;}
  void add(float v) {add(uint64_t(std::bit_cast<uint32_t>(v)));}
  void add(const std::string &v) {add(uint64_t(v.size()));for(unsigned char c:v) add(uint64_t(c));}
  void add(const ir::Transform &t) {for(float v:t.value) add(v);}
  void points(const std::vector<ir::Vec3> &points) {add(uint64_t(points.size()));for(auto p:points) {add(p.x);add(p.y);add(p.z);}}
  void topology(const ir::Mesh &m) {add(uint64_t(m.triangles.size()));for(const auto &t:m.triangles) {for(auto v:t.vertices) add(uint64_t(v));add(uint64_t(t.source_polygon));}add(uint64_t(m.graft_hidden_polygons.size()));for(auto p:m.graft_hidden_polygons) add(uint64_t(p));}
};
}
