#pragma once
#include "editor/document.h"

namespace dfv::editor {
struct HDBakeOptions {int color=1;std::string size="Medium",glands="NotObviousGlands";};
struct HDBakeJob {
  std::filesystem::path executable,directory;
  std::vector<std::filesystem::path> arguments;
  // 每个输入先转换为 4096 方图；无贴图的通道使用产品的中性底图。
  std::vector<std::filesystem::path> inputs;
  nlohmann::json identity;
};
std::filesystem::path hd_product_root(const std::vector<std::filesystem::path> &roots);
nlohmann::json hd_texture_identity(const Document &,const Snapshot &,size_t selected);
std::vector<std::filesystem::path> find_hd_textures(const Document &,const Snapshot &,size_t selected,const std::vector<std::filesystem::path> &roots,const std::filesystem::path &cache);
size_t copy_golden_palace_materials(Document &,Snapshot &,size_t selected,std::optional<size_t> only_instance={});
size_t apply_hd_textures(Document &,Snapshot &,size_t selected,const std::filesystem::path &directory,const std::vector<std::filesystem::path> &roots,std::optional<size_t> only_instance={});
HDBakeJob prepare_hd_bake(const Document &,const Snapshot &,size_t selected,const std::vector<std::filesystem::path> &roots,const std::filesystem::path &directory,const HDBakeOptions &,bool require_program=true);
bool hd_primary_channel(const std::string &id);
void finish_hd_bake(const HDBakeJob &);
enum class GeograftMaterialStrategy {skin,hd_nipples,golden_palace};
GeograftMaterialStrategy geograft_material_strategy(const Document &,size_t target);
struct GeograftMaterialUpdate {
  size_t updated=0,fallback=0;
  std::string strategy_error;
  std::vector<std::string> unresolved;
};
// 只更新指定附件；专用策略失败后按表面复制宿主当前皮肤，保留已成功的部位。
GeograftMaterialUpdate update_geograft_materials(Document &,Snapshot &,size_t target,const std::vector<std::filesystem::path> &roots,const std::filesystem::path &hd_directory={},const std::string &preparation_error={});
}
