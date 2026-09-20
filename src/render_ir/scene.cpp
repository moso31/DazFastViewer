#include "render_ir/scene.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dfv::ir {
Vec3 Transform::point(Vec3 p) const {
  return {value[0]*p.x+value[1]*p.y+value[2]*p.z+value[3],
          value[4]*p.x+value[5]*p.y+value[6]*p.z+value[7],
          value[8]*p.x+value[9]*p.y+value[10]*p.z+value[11]};
}
Transform Transform::translate(Vec3 p) {Transform t;t.value[3]=p.x;t.value[7]=p.y;t.value[11]=p.z;return t;}
Transform operator*(const Transform &a,const Transform &b) {
  Transform out;
  for(int r=0;r<3;++r) for(int c=0;c<4;++c) {
    out.value[r*4+c]=c==3?a.value[r*4+3]:0;
    for(int k=0;k<3;++k) out.value[r*4+c]+=a.value[r*4+k]*b.value[k*4+c];
  }
  return out;
}
void Bounds::add(Vec3 p) {
  if(empty) {minimum=maximum=p;empty=false;return;}
  minimum={std::min(minimum.x,p.x),std::min(minimum.y,p.y),std::min(minimum.z,p.z)};
  maximum={std::max(maximum.x,p.x),std::max(maximum.y,p.y),std::max(maximum.z,p.z)};
}
Vec3 Bounds::center() const {return {(minimum.x+maximum.x)*.5f,(minimum.y+maximum.y)*.5f,(minimum.z+maximum.z)*.5f};}
float Bounds::extent() const {return std::max({maximum.x-minimum.x,maximum.y-minimum.y,maximum.z-minimum.z});}
Bounds Scene::bounds() const {
  Bounds b;
  for(const auto &instance:instances) for(const auto p:meshes.at(instance.mesh).positions) b.add(instance.transform.point(p));
  return b;
}
void validate(const Camera &camera) {
  if(camera.width<=0 || camera.height<=0 || !std::isfinite(camera.fov) || camera.fov<=0 || camera.fov>=3.14f)
    throw std::runtime_error("IR: 无效相机");
  for(float f:camera.transform.value) if(!std::isfinite(f)) throw std::runtime_error("IR: 相机矩阵包含非有限值");
}
void validate(const Material &m,size_t texture_count) {
  for(float f:{m.base_color.x,m.base_color.y,m.base_color.z,m.roughness,m.metallic,m.opacity,m.transmission,m.ior,m.normal_strength})
    if(!std::isfinite(f)) throw std::runtime_error("IR: 无效材质参数");
  for(int t:{m.color_texture,m.roughness_texture,m.opacity_texture,m.normal_texture})
    if(t< -1 || (t>=0 && size_t(t)>=texture_count)) throw std::runtime_error("IR: 贴图索引越界");
}
void Scene::validate() const {
  auto require=[](bool valid,const char *reason) {if(!valid) throw std::runtime_error(reason);};
  auto finite=[](Vec3 p) {return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
  ir::validate(camera);
  require(finite(environment),"IR: 无效环境颜色");
  for(const auto &m:materials) ir::validate(m,textures.size());
  for(const auto &mesh:meshes) {
    require(!mesh.positions.empty() && !mesh.material_slots.empty(),"IR: 网格为空或没有材质槽");
    for(auto p:mesh.positions) require(finite(p),"IR: 顶点包含非有限值");
    for(const auto &t:mesh.triangles) {
      require(t.material_slot<mesh.material_slots.size(),"IR: 三角形材质槽越界");
      for(auto v:t.vertices) require(v<mesh.positions.size(),"IR: 顶点索引越界");
      for(auto uv:t.uv) require(std::isfinite(uv.x)&&std::isfinite(uv.y),"IR: UV 包含非有限值");
    }
  }
  for(const auto &i:instances) {
    require(i.mesh<meshes.size(),"IR: 实例网格索引越界");
    require(i.materials.size()==meshes[i.mesh].material_slots.size(),"IR: 实例材质槽数量不匹配");
    for(auto m:i.materials) require(m<materials.size(),"IR: 实例材质索引越界");
    for(float f:i.transform.value) require(std::isfinite(f),"IR: 实例矩阵包含非有限值");
  }
  for(const auto &light:lights) {
    require(finite(light.power) && std::isfinite(light.width) && std::isfinite(light.height) && light.width>0 && light.height>0,"IR: 无效灯光");
    for(float f:light.transform.value) require(std::isfinite(f),"IR: 灯光矩阵包含非有限值");
  }
}
void add_studio(Scene &scene) {
  const auto b=scene.bounds();if(b.empty) return;
  const float s=std::max(b.extent(),.05f);const auto c=b.center();
  Material floor;floor.id="preview-floor";floor.base_color={.18f,.18f,.18f};floor.roughness=.8f;
  const auto mat=uint32_t(scene.materials.size());scene.materials.push_back(floor);
  Mesh mesh;mesh.id="preview-floor";mesh.material_slots={floor.id};mesh.smooth=false;
  const float z=b.minimum.z-.003f*s;
  mesh.positions={{c.x-3*s,c.y-3*s,z},{c.x+3*s,c.y-3*s,z},{c.x+3*s,c.y+3*s,z},{c.x-3*s,c.y+3*s,z}};
  Triangle a,btri;a.vertices={0,1,2};btri.vertices={0,2,3};mesh.triangles={a,btri};
  Instance instance;instance.id=mesh.id;instance.mesh=uint32_t(scene.meshes.size());instance.materials={mat};
  scene.meshes.push_back(std::move(mesh));scene.instances.push_back(std::move(instance));
  for(int i=0;i<3;++i) {
    AreaLight light;light.id="preview-light-"+std::to_string(i);
    light.transform=Transform::translate({c.x+(i==0?-1.2f:1.2f)*s,c.y+(i==2?1.0f:-1.2f)*s,b.maximum.z+s});
    // 按面积缩放归一化功率，使厘米级道具与米级角色保持相近曝光。
    const float power=(i==0?20.0f:i==1?12.0f:16.0f)*s*s;
    light.width=light.height=s;light.power={power,.95f*power,.9f*power};scene.lights.push_back(light);
  }
}
}
