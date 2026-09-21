#include "cycles/adapter.h"
#include "scene/scene.h"
#include "scene/camera.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/background.h"
#include "scene/light.h"
#include "scene/attribute.h"
#include "util/colorspace.h"
#include "util/transform.h"
#include <map>
#include <stdexcept>
#include <cmath>

namespace dfv {
static ccl::float3 vector(ir::Vec3 v) {return ccl::make_float3(v.x,v.y,v.z);}
static ccl::Transform transform(const ir::Transform &t) {
  ccl::Transform out;const auto &v=t.value;
  out.x=ccl::make_float4(v[0],v[1],v[2],v[3]);out.y=ccl::make_float4(v[4],v[5],v[6],v[7]);out.z=ccl::make_float4(v[8],v[9],v[10],v[11]);return out;
}
void CyclesAdapter::material(ccl::Shader &shader,const ir::Material &m) {
  using namespace ccl;
  auto graph=make_unique<ShaderGraph>();auto *bsdf=graph->create_node<PrincipledBsdfNode>();
  bsdf->set_base_color(vector(m.base_color));bsdf->set_roughness(m.roughness);bsdf->set_metallic(m.metallic);
  bsdf->set_alpha(m.opacity);bsdf->set_transmission_weight(m.transmission);bsdf->set_ior(m.ior);
  auto texture=[&](int index) {
    const auto &t=textures_.at(index);auto *node=graph->create_node<ImageTextureNode>();
    auto *uv=graph->create_node<UVMapNode>();uv->set_attribute(ustring("UVMap"));
    graph->connect(uv->output("UV"),node->input("Vector"));
    const auto utf8=t.file.u8string();node->set_filename(ustring(std::string(utf8.begin(),utf8.end())));
    // 本应用固定在线性 Rec.709 渲染。保留贴图的 sRGB 编码，交由 SVM 解码一次。
    // 此版本的 __builtin_srgb CPU 路径会执行编码，不能作为这里的输入解码空间。
    node->set_colorspace(t.colorspace==ir::ColorSpace::srgb?u_colorspace_scene_linear_srgb:u_colorspace_data);
    return node;
  };
  if(m.color_texture>=0) {
    auto *image=texture(m.color_texture);
    auto *multiply=graph->create_node<VectorMathNode>();multiply->set_math_type(NODE_VECTOR_MATH_MULTIPLY);
    multiply->set_vector2(vector(m.base_color));
    graph->connect(image->output("Color"),multiply->input("Vector1"));graph->connect(multiply->output("Vector"),bsdf->input("Base Color"));
  }
  auto scalar_texture=[&](int index,float factor,const char *socket) {
    if(index<0) return;
    auto *image=texture(index);auto *multiply=graph->create_node<MathNode>();
    multiply->set_math_type(NODE_MATH_MULTIPLY);multiply->set_value2(factor);
    graph->connect(image->output("Color"),multiply->input("Value1"));graph->connect(multiply->output("Value"),bsdf->input(socket));
  };
  scalar_texture(m.roughness_texture,m.roughness,"Roughness");scalar_texture(m.opacity_texture,m.opacity,"Alpha");
  ShaderOutput *surface_normal=nullptr;
  if(m.normal_texture>=0) {
    auto *image=texture(m.normal_texture);auto *normal=graph->create_node<NormalMapNode>();
    normal->set_strength(m.normal_strength);normal->set_attribute(ustring("UVMap"));
    graph->connect(image->output("Color"),normal->input("Color"));surface_normal=normal->output("Normal");
  }
  if(m.bump_texture>=0 && m.bump_strength>0 && m.bump_distance>0) {
    auto *image=texture(m.bump_texture);auto *bump=graph->create_node<BumpNode>();
    bump->set_strength(m.bump_strength);bump->set_distance(m.bump_distance);
    graph->connect(image->output("Color"),bump->input("Height"));
    if(surface_normal) graph->connect(surface_normal,bump->input("Normal"));
    surface_normal=bump->output("Normal");
  }
  if(surface_normal) graph->connect(surface_normal,bsdf->input("Normal"));
  graph->connect(bsdf->output("BSDF"),graph->output()->input("Surface"));
  shader.name=ustring(m.id);shader.set_graph(std::move(graph));shader.tag_update(&scene_);
}
void CyclesAdapter::load(const ir::Scene &source) {
  using namespace ccl;
  if(loaded_) throw std::runtime_error("同一 CyclesAdapter 只允许一次完整加载，请使用 Delta 更新");
  source.validate();textures_=source.textures;stats_.textures=textures_.size();
  meshes_.resize(source.meshes.size());for(const auto &m:source.meshes) vertex_counts_.push_back(m.positions.size());
  for(const auto &m:source.materials) {auto *shader=scene_.create_node<Shader>();material(*shader,m);shaders_.push_back(shader);++stats_.materials;}
  std::map<std::pair<uint32_t,std::vector<uint32_t>>,Mesh *> meshes;
  for(const auto &instance:source.instances) {
    const auto key=std::make_pair(instance.mesh,instance.materials);auto found=meshes.find(key);Mesh *mesh;
    const auto &data=source.meshes.at(instance.mesh);
    if(found==meshes.end()) {
      mesh=scene_.create_node<Mesh>();mesh->resize_mesh(int(data.positions.size()),int(data.triangles.size()));
      auto *positions=mesh->get_position_for_write();
      for(size_t i=0;i<data.positions.size();++i) positions[i]=vector(data.positions[i]);
      auto *uv=mesh->attributes.add(ATTR_STD_UV,ustring("UVMap"))->data_for_write<float2>();
      for(size_t i=0;i<data.triangles.size();++i) {
        const auto &t=data.triangles[i];
        for(size_t k=0;k<3;++k) {mesh->get_triangles()[i*3+k]=int(t.vertices[k]);uv[i*3+k]=make_float2(t.uv[k].x,t.uv[k].y);}
        mesh->get_shader()[i]=t.material_slot;mesh->get_smooth()[i]=data.smooth;
      }
      array<Node *> shaders(instance.materials.size());for(size_t i=0;i<shaders.size();++i) shaders[i]=shaders_.at(instance.materials[i]);mesh->set_used_shaders(shaders);
      meshes.emplace(key,mesh);meshes_[instance.mesh].push_back(mesh);++stats_.meshes;stats_.unique_triangles+=data.triangles.size();
    } else mesh=found->second;
    auto *object=scene_.create_node<Object>();object->name=ustring(instance.id);object->set_geometry(mesh);object->set_tfm(transform(instance.transform));
    objects_.push_back(object);
    ++stats_.instances;stats_.triangles+=data.triangles.size();
  }
  auto light_graph=make_unique<ShaderGraph>();auto *emission=light_graph->create_node<EmissionNode>();
  emission->set_color(one_float3());emission->set_strength(1);light_graph->connect(emission->output("Emission"),light_graph->output()->input("Surface"));
  scene_.default_light->set_graph(std::move(light_graph));scene_.default_light->tag_update(&scene_);
  for(const auto &data:source.lights) {
    Light *light=nullptr;
    if(data.kind==ir::LightKind::point) light=scene_.create_node<PointLight>();
    else if(data.kind==ir::LightKind::spot) {auto *spot=scene_.create_node<SpotLight>();spot->set_angle(data.angle);light=spot;}
    else if(data.kind==ir::LightKind::distant) light=scene_.create_node<SunLight>();
    else {auto *area=scene_.create_node<AreaLight>();area->set_sizeu(data.width);area->set_sizev(data.height);light=area;}
    light->set_strength(vector(data.power));light->set_use_mis(true);lights_.push_back(light);
    auto *object=scene_.create_node<Object>();object->set_geometry(light);object->set_tfm(transform(data.transform));
    light_objects_.push_back(object);
  }
  auto graph=make_unique<ShaderGraph>();auto *bg=graph->create_node<BackgroundNode>();bg->set_color(vector(source.environment));bg->set_strength(1);
  graph->connect(bg->output("Background"),graph->output()->input("Surface"));scene_.default_background->set_graph(std::move(graph));scene_.default_background->tag_update(&scene_);
  loaded_=true;ir::Delta initial;initial.camera=source.camera;apply(initial);
}
void CyclesAdapter::apply(const ir::Delta &delta) {
  if(!loaded_) throw std::runtime_error("CyclesAdapter 尚未加载场景");
  // 先校验整个变更，避免索引错误导致只应用一部分。
  if(delta.camera) ir::validate(*delta.camera);
  for(const auto &edit:delta.lights) {
    if(edit.index>=lights_.size()) throw std::runtime_error("灯光索引越界");
    ir::Scene check;check.lights.push_back(edit.value);check.validate();
  }
  for(const auto &edit:delta.materials) {
    if(edit.index>=shaders_.size()) throw std::runtime_error("材质更新索引越界");
    ir::validate(edit.value,textures_.size());
  }
  for(const auto &edit:delta.meshes) {
    if(edit.index>=meshes_.size() || edit.positions.size()!=vertex_counts_[edit.index]) throw std::runtime_error("顶点 Delta 不能改变拓扑或越界");
    for(const auto &p:edit.positions) if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) throw std::runtime_error("顶点 Delta 含非有限值");
    for(const auto *mesh:meshes_[edit.index]) if(mesh->transform_applied) throw std::runtime_error("动态编辑要求未烘焙对象变换的动态 BVH 场景");
  }
  for(const auto &edit:delta.instances) {
    if(edit.index>=objects_.size()) throw std::runtime_error("实例 Delta 索引越界");
    for(float x:edit.transform.value) if(!std::isfinite(x)) throw std::runtime_error("实例变换含非有限值");
    if(objects_[edit.index]->get_geometry()->transform_applied) throw std::runtime_error("对象变换已烘焙，不能直接动态修改");
  }
  if(delta.camera) {
    const auto &c=*delta.camera;auto &camera=*scene_.camera;
    camera.set_camera_type(ccl::CAMERA_PERSPECTIVE);camera.set_full_width(c.width);camera.set_full_height(c.height);
    camera.set_fov(c.fov);camera.set_matrix(transform(c.transform));camera.compute_auto_viewplane();
    camera.need_device_update=true;camera.need_flags_update=true;++stats_.camera_updates;
  }
  for(const auto &edit:delta.materials) {material(*shaders_.at(edit.index),edit.value);++stats_.material_updates;}
  for(const auto &edit:delta.lights) {
    lights_[edit.index]->set_strength(vector(edit.value.power));lights_[edit.index]->tag_update(&scene_);
    auto *object=light_objects_[edit.index];object->set_tfm(transform(edit.value.transform));object->tag_update(&scene_);
  }
  for(const auto &edit:delta.meshes) {
    for(auto *mesh:meshes_[edit.index]) {
      auto *positions=mesh->get_position_for_write();
      for(size_t i=0;i<edit.positions.size();++i) positions[i]=vector(edit.positions[i]);
      mesh->attributes.remove(ccl::ATTR_STD_VERTEX_NORMAL);mesh->tag_position_modified();
      mesh->compute_bounds();mesh->tag_update(&scene_,false);
    }
    ++stats_.geometry_updates;
  }
  for(const auto &edit:delta.instances) {
    auto *object=objects_[edit.index];object->set_tfm(transform(edit.transform));object->tag_update(&scene_);++stats_.instance_updates;
  }
}
}
