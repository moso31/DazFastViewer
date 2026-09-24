#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dfv::ir {
struct Vec2 {float x=0,y=0;bool operator==(const Vec2 &) const = default;};
struct Vec3 {float x=0,y=0,z=0;bool operator==(const Vec3 &) const = default;};
// 保存 DAZ 碰撞修改器的显式对象关系；距离统一为米。
struct MeshSmoothing {
  bool enabled=false;
  std::string collision_target;
  int smoothing_iterations=2,collision_iterations=3;
  float weight=.5f;
  bool operator==(const MeshSmoothing &) const = default;
};
// 米，右手坐标系，Z 向上；矩阵为三行四列，作用于列向量。
struct Transform {
  std::array<float,12> value{1,0,0,0, 0,1,0,0, 0,0,1,0};
  Vec3 point(Vec3 p) const;
  static Transform translate(Vec3 p);
  bool operator==(const Transform &) const = default;
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
struct ImageLayer {
  std::filesystem::path file;
  std::string operation="alpha_blend";
  Vec3 color{0,0,0};
  float opacity=1,rotation=0;
  Vec2 scale{1,1},offset;
  bool invert=false,mirror_x=false,mirror_y=false;
  bool operator==(const ImageLayer &) const = default;
};
struct Texture {
  std::string id;std::filesystem::path file;ColorSpace colorspace=ColorSpace::srgb;
  // LIE 在图像编码空间叠加，最后统一转换到工作空间；源资产保持只读。
  std::vector<ImageLayer> layers;
  float gamma=0;
  bool operator==(const Texture &) const = default;
};
struct Material {
  std::string id;
  Vec3 base_color{0.5f,0.5f,0.5f};
  float roughness=.5f,metallic=0,opacity=1,transmission=0,ior=1.5f,normal_strength=1;
  // 世界空间高度范围，单位为米；与法线贴图叠加。
  float bump_strength=0,bump_distance=.001f;
  int color_texture=-1,roughness_texture=-1,opacity_texture=-1,normal_texture=-1;
  int bump_texture=-1;
  int displacement_texture=-1;
  float displacement_strength=0,displacement_min=-.001f,displacement_max=.001f;
  bool thin_walled=false,hair=false,roughness_from_glossiness=false,weighted_glossy=false;
  float specular=.5f,anisotropy=0,anisotropy_rotation=0,translucency=0;
  float subsurface=0,subsurface_anisotropy=0;
  Vec3 subsurface_radius{.001f,.001f,.001f},translucency_color{1,1,1};
  Vec3 subsurface_color{1,1,1};
  bool separate_subsurface_color=false;
  float coat=0,coat_roughness=.1f,coat_ior=1.5f;
  int coat_mode=2;float coat_normal=.04f,coat_grazing=1,coat_exponent=5;
  Vec3 coat_color{1,1,1},specular_color{1,1,1};
  float dual_weight=0,dual_ratio=.5f,dual_roughness1=.3f,dual_roughness2=.6f,dual_specular=.5f;
  float hair_root_radius=.00005f,hair_tip_radius=.000025f,hair_radial_roughness=.3f;
  Vec3 hair_tip_color{.1f,.1f,.1f};
  float hair_melanin=0,hair_redness=.5f;
  Vec2 uv_scale{1,1},uv_offset;
  int specular_texture=-1,translucency_texture=-1,translucency_color_texture=-1;
  int coat_texture=-1,coat_roughness_texture=-1,dual_texture=-1,metallic_texture=-1,transmission_texture=-1;
  int specular_color_texture=-1,coat_color_texture=-1;
  bool bump_invert=false,bump_from_texel_density=false;
  bool operator==(const Material &) const = default;
};
template<class M> auto texture_indices(M &m) {
  return std::array{&m.color_texture,&m.roughness_texture,&m.opacity_texture,&m.normal_texture,&m.bump_texture,
    &m.specular_texture,&m.translucency_texture,&m.translucency_color_texture,&m.coat_texture,&m.coat_roughness_texture,
    &m.dual_texture,&m.metallic_texture,&m.transmission_texture,&m.specular_color_texture,&m.coat_color_texture,&m.displacement_texture};
}
struct Triangle {
  std::array<uint32_t,3> vertices{};
  std::array<Vec2,3> uv{};
  uint32_t material_slot=0;
  uint32_t polygon_group=0;
  uint32_t source_polygon=0;
  bool operator==(const Triangle &) const = default;
};
// 发丝保持源顶点编号，复用 Morph / 蒙皮；后端按曲线展开控制点。
struct Curve {
  std::vector<uint32_t> vertices;
  uint32_t material_slot=0;
  Vec2 uv;
  bool operator==(const Curve &) const = default;
};
struct SubdivisionSettings {
  bool enabled=false;
  int level=0,render_level=0;
  int algorithm=0,edge_interpolation=2,normal_smoothing=0;
  bool operator==(const SubdivisionSettings &) const = default;
};
struct Polygon {
  std::vector<uint32_t> vertices;
  std::vector<Vec2> uv;
  uint32_t material_slot=0,polygon_group=0;
  bool operator==(const Polygon &) const = default;
};
struct Mesh {
  // GeoGraft 的目标基础拓扑；用于把附加表面纳入服装碰撞。
  uint32_t graft_target_vertices=0;
  uint32_t graft_target_polygons=0,source_polygon_count=0;
  std::vector<uint32_t> graft_hidden_polygons;
  // DSON 顺序：插件顶点、宿主顶点；最终形变后锁定接缝。
  std::vector<std::array<uint32_t,2>> graft_vertex_pairs;
  // 原拓扑供 Morph/蒙皮核对；遮盖仅影响绘制和选取，Visible 不恢复宿主面。
  std::vector<uint32_t> hidden_polygons;
  std::string id;
  std::vector<Vec3> positions;
  std::vector<Triangle> triangles;
  // 细分必须使用原始四边面，不能对绘制用的三角化结果做 Catmark。
  std::vector<Polygon> polygons;
  SubdivisionSettings subdivision;
  struct Crease {uint32_t a,b;float weight;bool operator==(const Crease &) const = default;};
  std::vector<Crease> creases;
  std::vector<std::pair<uint32_t,float>> corners;
  std::vector<std::string> material_slots;
  std::vector<std::string> polygon_groups;
  bool smooth=true;
  std::vector<Curve> curves;
  bool draws(const Triangle &t) const {return !std::binary_search(hidden_polygons.begin(),hidden_polygons.end(),t.source_polygon);}
};
struct Instance {
  std::string id;
  uint32_t mesh=0;
  Transform transform;
  std::vector<uint32_t> materials;
  bool visible=true;
  // DAZ 实例复用最终形变网格；不为散布条目重复发现参数或蒙皮。
  int prototype=-1;
  // GeoGraft 的直接宿主实例；与可编辑对象身份独立，供渲染侧共同细分。
  int graft_source=-1;
  std::string instance_node;
  // 一个 DAZ Instance / 散布条目的所有渲染零件共用此键。
  std::string instance_group,instance_label;
};
enum class LightKind {area,point,spot,distant};
struct AreaLight {
  std::string id;
  Transform transform;
  Vec3 power{500,500,500};
  float width=1,height=1;
  LightKind kind=LightKind::area;
  float angle=.785398f;
  bool operator==(const AreaLight &) const = default;
};
struct Camera {
  Transform transform;
  int width=1600,height=900;
  // 弧度，沿较短画面边的视场角；与 Cycles auto viewplane 约定一致。
  float fov=.8f;
  uint64_t revision=1;
};
struct Option {
  std::string id,label,group,type,image_uri;
  std::vector<double> value;
  std::vector<std::string> choices;
  double minimum=-10000,maximum=10000,step=.01;
  bool visible=true,supported=false;
  bool operator==(const Option &) const = default;
};
struct OptionNode {
  std::string id,label;
  std::vector<Option> parameters;
  bool operator==(const OptionNode &) const = default;
};
struct RenderOptions {
  OptionNode environment,tonemapper;
  std::filesystem::path environment_file;
  std::array<float,3> backdrop{.055f,.055f,.055f};
  bool operator==(const RenderOptions &) const = default;
};
struct Scene {
  RenderOptions options;
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
struct VisibilityEdit {uint32_t index=0;bool visible=true;};
struct LightEdit {uint32_t index=0;AreaLight value;};
struct Delta {
  std::optional<RenderOptions> options;
  std::optional<Camera> camera;
  std::vector<MaterialEdit> materials;
  std::vector<MeshEdit> meshes;
  std::vector<InstanceEdit> instances;
  std::vector<LightEdit> lights;
  std::vector<VisibilityEdit> visibility;
};
void validate(const Camera &camera);
void validate(const Material &material,size_t texture_count);
// 用于无自带灯光的资产预览；不是原 DUF 的灯光语义。
void add_studio(Scene &scene);
}
