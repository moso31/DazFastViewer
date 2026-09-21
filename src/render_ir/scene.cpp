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
Transform inverse(const Transform &m) {
  const auto &a=m.value;const double determinant=double(a[0])*(double(a[5])*a[10]-double(a[6])*a[9])-double(a[1])*(double(a[4])*a[10]-double(a[6])*a[8])+double(a[2])*(double(a[4])*a[9]-double(a[5])*a[8]);
  if(!std::isfinite(determinant)||std::abs(determinant)<1e-15) throw std::runtime_error("Fit To 的绑定矩阵不可逆");
  ir::Transform result;auto &r=result.value;
  r[0]=float((double(a[5])*a[10]-double(a[6])*a[9])/determinant);r[1]=float((double(a[2])*a[9]-double(a[1])*a[10])/determinant);r[2]=float((double(a[1])*a[6]-double(a[2])*a[5])/determinant);
  r[4]=float((double(a[6])*a[8]-double(a[4])*a[10])/determinant);r[5]=float((double(a[0])*a[10]-double(a[2])*a[8])/determinant);r[6]=float((double(a[2])*a[4]-double(a[0])*a[6])/determinant);
  r[8]=float((double(a[4])*a[9]-double(a[5])*a[8])/determinant);r[9]=float((double(a[1])*a[8]-double(a[0])*a[9])/determinant);r[10]=float((double(a[0])*a[5]-double(a[1])*a[4])/determinant);
  const auto offset=result.point({-a[3],-a[7],-a[11]});r[3]=offset.x;r[7]=offset.y;r[11]=offset.z;return result;
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
  for(const auto &instance:instances) if(instance.visible) for(const auto p:meshes.at(instance.mesh).positions) b.add(instance.transform.point(p));
  return b;
}
void validate(const Camera &camera) {
  if(camera.width<=0 || camera.height<=0 || !std::isfinite(camera.fov) || camera.fov<=0 || camera.fov>=3.14f)
    throw std::runtime_error("IR: 无效相机");
  for(float f:camera.transform.value) if(!std::isfinite(f)) throw std::runtime_error("IR: 相机矩阵包含非有限值");
}
void validate(const Material &m,size_t texture_count) {
  for(float f:{m.base_color.x,m.base_color.y,m.base_color.z,m.roughness,m.metallic,m.opacity,m.transmission,m.ior,m.normal_strength,m.bump_strength,m.bump_distance})
    if(!std::isfinite(f)) throw std::runtime_error("IR: 无效材质参数");
  for(float f:{m.specular,m.anisotropy,m.anisotropy_rotation,m.translucency,m.subsurface,m.subsurface_anisotropy,m.coat,m.coat_roughness,m.coat_ior,
      m.dual_weight,m.dual_ratio,m.dual_roughness1,m.dual_roughness2,m.dual_specular,m.hair_root_radius,m.hair_tip_radius,m.hair_radial_roughness,m.hair_melanin,m.hair_redness,m.uv_scale.x,m.uv_scale.y,m.uv_offset.x,m.uv_offset.y})
    if(!std::isfinite(f)) throw std::runtime_error("IR: 扩展材质参数无效");
  for(auto v:{m.subsurface_radius,m.translucency_color,m.coat_color,m.specular_color,m.hair_tip_color}) for(float f:{v.x,v.y,v.z})
    if(!std::isfinite(f)||f<0) throw std::runtime_error("IR: 材质颜色或散射半径无效");
  if(m.hair_root_radius<0||m.hair_tip_radius<0) throw std::runtime_error("IR: 发丝半径不能为负");
  if(m.bump_strength<0 || m.bump_distance<0) throw std::runtime_error("IR: 凹凸强度或距离不能为负");
  for(const auto *t:texture_indices(m))
    if(*t< -1 || (*t>=0 && size_t(*t)>=texture_count)) throw std::runtime_error("IR: 贴图索引越界");
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
    for(const auto &curve:mesh.curves) {
      require(curve.vertices.size()>=2&&curve.material_slot<mesh.material_slots.size(),"IR: 发丝长度或材质槽无效");
      for(auto v:curve.vertices) require(v<mesh.positions.size(),"IR: 发丝顶点索引越界");
      require(std::isfinite(curve.uv.x)&&std::isfinite(curve.uv.y),"IR: 发丝 UV 无效");
    }
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
