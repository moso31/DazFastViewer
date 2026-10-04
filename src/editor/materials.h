#pragma once
#include "render_ir/scene.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <map>
#include <set>

namespace dfv::editor {
using MaterialPatch=std::map<std::string,nlohmann::json>;
// 使用实例与表面名称，不保存会在追加/删除后改变的资源索引。
using MaterialOverrides=std::map<std::string,std::map<std::string,MaterialPatch>>;
struct MaterialSurface {size_t instance=0,slot=0;bool operator==(const MaterialSurface &)const=default;};
struct MaterialParameter {
  enum Kind {number,color,vector,boolean,choice,texture};
  std::string id,label,group;
  Kind kind=number;
  double minimum=0,maximum=1,step=.01;
  std::vector<std::string> choices;
  std::function<nlohmann::json(const ir::Material &)> read;
  std::function<void(ir::Material &,const nlohmann::json &)> write;
  // Restore valid imported values without imposing the editor's input range.
  std::function<void(ir::Material &,const nlohmann::json &)> restore;
  std::function<void(ir::Material &,const ir::Material &)> copy;
  int ir::Material::*texture_member=nullptr;
  ir::ColorSpace colorspace=ir::ColorSpace::linear;
};
const std::vector<MaterialParameter> &material_parameters();
const MaterialParameter &material_parameter(const std::string &id);
std::set<std::string> material_preset_parameters(const ir::Material &preset,const ir::Material *context=nullptr);
nlohmann::json material_value(const ir::Material &material,const std::vector<ir::Texture> &textures,const MaterialParameter &parameter);
void set_material_value(ir::Material &material,std::vector<ir::Texture> &textures,const MaterialParameter &parameter,const nlohmann::json &value);
ir::Material effective_material(const ir::Scene &source,const MaterialOverrides &overrides,size_t instance,size_t slot,std::vector<ir::Texture> &textures);
nlohmann::json copy_material(const ir::Scene &scene,const MaterialOverrides &overrides,MaterialSurface surface);
ir::Material read_copied_material(const nlohmann::json &copy,std::vector<ir::Texture> &textures);
// 只改材质/贴图/绑定，保留正在形变的几何、灯光和相机。返回绑定或资源布局是否改变。
bool apply_material_overrides(ir::Scene &scene,const ir::Scene &source,const MaterialOverrides &overrides,ir::Delta *delta=nullptr);
void validate_material_overrides(const ir::Scene &source,const MaterialOverrides &overrides);
void prune_material_overrides(const ir::Scene &source,MaterialOverrides &overrides);
}
