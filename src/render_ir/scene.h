#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dfv::ir {
struct Vec2 {float x=0,y=0;};
struct Vec3 {float x=0,y=0,z=0;};
// 米，右手坐标系，Z 向上；矩阵为三行四列，作用于列向量。
struct Transform {
  std::array<float,12> value{1,0,0,0, 0,1,0,0, 0,0,1,0};
  Vec3 point(Vec3 p) const;
  static Transform translate(Vec3 p);
};
Transform operator*(const Transform &a,const Transform &b);
Transform inverse(const Transform &transform);
struct Bounds {
  Vec3 minimum{},maximum{};
  bool empty=true;
  void add(Vec3 p);
  Vec3 center() const;
  float extent() const;
};
enum class ColorSpace {srgb,linear};
struct Texture {std::string id;std::filesystem::path file;ColorSpace colorspace=ColorSpace::srgb;};
struct Material {
  std::string id;
  Vec3 base_color{0.5f,0.5f,0.5f};
  float roughness=.5f,metallic=0,opacity=1,transmission=0,ior=1.5f,normal_strength=1;
  // 世界空间高度范围，单位为米；与法线贴图叠加。
  float bump_strength=0,bump_distance=.001f;
  int color_texture=-1,roughness_texture=-1,opacity_texture=-1,normal_texture=-1;
  int bump_texture=-1;
};
struct Triangle {
  std::array<uint32_t,3> vertices{};
  std::array<Vec2,3> uv{};
  uint32_t material_slot=0;
  uint32_t polygon_group=0;
};
struct Mesh {
  std::string id;
  std::vector<Vec3> positions;
  std::vector<Triangle> triangles;
  std::vector<std::string> material_slots;
  std::vector<std::string> polygon_groups;
  bool smooth=true;
};
struct Instance {
  std::string id;
  uint32_t mesh=0;
  Transform transform;
  std::vector<uint32_t> materials;
};
enum class LightKind {area,point,spot,distant};
struct AreaLight {
  std::string id;
  Transform transform;
  Vec3 power{500,500,500};
  float width=1,height=1;
  LightKind kind=LightKind::area;
  float angle=.785398f;
};
struct Camera {
  Transform transform;
  int width=1600,height=900;
  // 弧度，沿较短画面边的视场角；与 Cycles auto viewplane 约定一致。
  float fov=.8f;
  uint64_t revision=1;
};
struct Scene {
  std::vector<Texture> textures;
  std::vector<Material> materials;
  std::vector<Mesh> meshes;
  std::vector<Instance> instances;
  std::vector<AreaLight> lights;
  Vec3 environment{.048f,.064f,.096f};
  Camera camera;
  void validate() const;
  Bounds bounds() const;
};
struct MaterialEdit {uint32_t index=0;Material value;};
struct MeshEdit {uint32_t index=0;std::vector<Vec3> positions;};
struct InstanceEdit {uint32_t index=0;Transform transform;};
struct LightEdit {uint32_t index=0;AreaLight value;};
struct Delta {
  std::optional<Camera> camera;
  std::vector<MaterialEdit> materials;
  std::vector<MeshEdit> meshes;
  std::vector<InstanceEdit> instances;
  std::vector<LightEdit> lights;
};
void validate(const Camera &camera);
void validate(const Material &material,size_t texture_count);
// 用于无自带灯光的资产预览；不是原 DUF 的灯光语义。
void add_studio(Scene &scene);
}
