#pragma once
#include "render_ir/options.h"
#include <nlohmann/json.hpp>
namespace dfv::ir {
inline nlohmann::json options_json(const RenderOptions &options) {
  using J=nlohmann::json;J result={{"schema","dfv-render-options-1"},{"backdrop",options.backdrop}};
  const auto path=options.environment_file.generic_u8string();result["environment_file"]=std::string(path.begin(),path.end());
  for(const auto &[key,node]:std::array<std::pair<const char *,const OptionNode *>,2>{{{"environment",&options.environment},{"tonemapper",&options.tonemapper}}}) {
    J n={{"id",node->id},{"label",node->label},{"parameters",J::array()}};
    for(const auto &p:node->parameters) n["parameters"].push_back({{"id",p.id},{"label",p.label},{"group",p.group},{"type",p.type},{"image_uri",p.image_uri},{"value",p.value},{"choices",p.choices},{"minimum",p.minimum},{"maximum",p.maximum},{"step",p.step},{"visible",p.visible},{"supported",p.supported}});
    result[key]=std::move(n);
  }return result;
}
inline RenderOptions options_from_json(const nlohmann::json &data) {
  if(data.at("schema")!="dfv-render-options-1") throw std::runtime_error("不支持的渲染选项格式");
  RenderOptions result;result.backdrop=data.at("backdrop").get<std::array<float,3>>();result.environment_file=std::filesystem::u8path(data.at("environment_file").get<std::string>());
  for(const auto &[key,node]:std::array<std::pair<const char *,OptionNode *>,2>{{{"environment",&result.environment},{"tonemapper",&result.tonemapper}}}) {
    const auto &n=data.at(key);node->id=n.at("id");node->label=n.at("label");
    for(const auto &j:n.at("parameters")) {Option p;p.id=j.at("id");p.label=j.at("label");p.group=j.at("group");p.type=j.at("type");p.image_uri=j.at("image_uri");p.value=j.at("value").get<std::vector<double>>();p.choices=j.at("choices").get<std::vector<std::string>>();p.minimum=j.at("minimum");p.maximum=j.at("maximum");p.step=j.at("step");p.visible=j.at("visible");p.supported=j.at("supported");
      if(!std::isfinite(p.minimum)||!std::isfinite(p.maximum)||p.minimum>p.maximum||!std::isfinite(p.step)) throw std::runtime_error("无效参数范围");
      for(auto v:p.value) if(!std::isfinite(v)) throw std::runtime_error("参数包含非有限值");node->parameters.push_back(std::move(p));
    }
  }return result;
}
}
