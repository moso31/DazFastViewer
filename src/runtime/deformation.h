#pragma once
#include "runtime/formula.h"
#include "runtime/morph.h"
#include "runtime/skeleton.h"
#include "runtime/conform.h"
#include <memory>

namespace dfv::runtime {
class DeformationRuntime {
  const std::vector<Target> &targets_;
  const std::vector<Skin> &skins_;
  const std::vector<FormulaGraph> &graphs_;
  MorphRuntime morph_;
  SkinningRuntime skin_;
  ConformRuntime conform_;
  std::vector<std::unique_ptr<FormulaRuntime>> formulas_;
  std::vector<Properties> previous_;
  std::vector<std::vector<JointPose>> previous_poses_;
  std::vector<std::vector<float>> effective_;
  std::vector<std::vector<JointPose>> effective_poses_;
  struct Attachment {size_t target,skin,joint;ir::Transform figure,inverse_bind;};
  std::vector<Attachment> attachments_;
  void feed(const std::vector<Properties> &values,const std::vector<std::vector<JointPose>> &poses,std::vector<std::vector<float>> &weights,std::vector<std::vector<JointPose>> &resolved);
public:
  DeformationRuntime(ir::Scene &scene,const std::vector<Target> &targets,const std::vector<Skin> &skins,const std::vector<FormulaGraph> &graphs);
  ir::Delta evaluate(const std::vector<Properties> &values,const std::vector<std::vector<JointPose>> &poses);
  const auto &morph_stats() const {return morph_.stats();}
  const auto &skin_stats() const {return skin_.stats();}
  FormulaStats formula_stats() const;
  const auto &conform_stats() const {return conform_.stats();}
  const auto &conform_links() const {return conform_.links();}
  const auto &effective() const {return effective_;}
  const auto &effective_poses() const {return effective_poses_;}
  const auto &formula_values(size_t target) const {return formulas_.at(target)->values();}
};
// Alias 与原参数共享编辑值；渲染后的 ERC 结果另行显示，不写回用户输入。
void set_parameter(const Target &target,Properties &values,size_t index,float value);
void sync_aliases(const Target &target,Properties &values);
}
