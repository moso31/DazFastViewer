#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include "local_shadow_types.h"

CCL_NAMESPACE_BEGIN

inline DfvLocalShadowNode dfv_shadow_empty_bounds()
{
  DfvLocalShadowNode node{};
  node.object = -1;
  for (int axis = 0; axis < 3; ++axis) {
    node.lower[axis] = std::numeric_limits<float>::infinity();
    node.upper[axis] = -std::numeric_limits<float>::infinity();
  }
  return node;
}

inline void dfv_shadow_grow(DfvLocalShadowNode &a, const DfvLocalShadowNode &b)
{
  for (int axis = 0; axis < 3; ++axis) {
    a.lower[axis] = std::min(a.lower[axis], b.lower[axis]);
    a.upper[axis] = std::max(a.upper[axis], b.upper[axis]);
  }
  a.visibility |= b.visibility;
}

inline double dfv_shadow_area(const DfvLocalShadowNode &node)
{
  const double x = double(node.upper[0]) - node.lower[0];
  const double y = double(node.upper[1]) - node.lower[1];
  const double z = double(node.upper[2]) - node.lower[2];
  return x*y + y*z + z*x;
}

// 用表面积启发式分离大对象与局部密集对象，避免巨型包围盒污染每个分支。
inline std::vector<DfvLocalShadowNode> dfv_build_shadow_tree(
    std::vector<DfvLocalShadowNode> leaves)
{
  std::vector<DfvLocalShadowNode> nodes;
  if (leaves.empty()) return nodes;
  nodes.reserve(leaves.size()*2-1);
  const auto build = [&](auto &&self, int begin, int end, int depth) -> void {
    const int index = int(nodes.size()), size = end-begin;
    auto bounds = dfv_shadow_empty_bounds();
    for (int i = begin; i < end; ++i) dfv_shadow_grow(bounds, leaves[i]);
    nodes.push_back(bounds);
    if (size == 1) {
      nodes[index].object = leaves[begin].object;
    }
    else {
      int best_axis = 0, best_split = size/2;
      double best_cost = std::numeric_limits<double>::infinity();
      const auto order = [](int axis) {
        return [axis](const auto &a, const auto &b) {
          const double ac = double(a.lower[axis])+a.upper[axis];
          const double bc = double(b.lower[axis])+b.upper[axis];
          return ac < bc || (ac == bc && a.object < b.object);
        };
      };
      // 限制递归深度；极端嵌套时改为平衡划分，GPU 查询仍无固定栈上限。
      if (depth < 32) {
        std::vector<DfvLocalShadowNode> suffix(size);
        for (int axis = 0; axis < 3; ++axis) {
          std::sort(leaves.begin()+begin, leaves.begin()+end, order(axis));
          auto right = dfv_shadow_empty_bounds();
          for (int i = size-1; i >= 0; --i) {
            dfv_shadow_grow(right, leaves[begin+i]);
            suffix[i] = right;
          }
          auto left = dfv_shadow_empty_bounds();
          for (int split = 1; split < size; ++split) {
            dfv_shadow_grow(left, leaves[begin+split-1]);
            const double cost = dfv_shadow_area(left)*split +
                                dfv_shadow_area(suffix[split])*(size-split);
            if (cost < best_cost) {
              best_cost = cost;
              best_axis = axis;
              best_split = split;
            }
          }
        }
      }
      std::sort(leaves.begin()+begin, leaves.begin()+end, order(best_axis));
      self(self, begin, begin+best_split, depth+1);
      self(self, begin+best_split, end, depth+1);
    }
    nodes[index].escape = int(nodes.size());
  };
  build(build, 0, int(leaves.size()), 0);
  return nodes;
}

CCL_NAMESPACE_END
