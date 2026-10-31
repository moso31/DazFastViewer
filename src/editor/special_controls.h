#pragma once
#include "editor/parameters.h"

namespace dfv::editor {
inline ParameterControl forefoot_control(std::function<double()> read,std::function<void(double)> write){
  ParameterControl c;c.id="special/forefoot_uniformity";c.label="前掌统一权重（%）";c.group="/特殊处理";c.minimum=c.slider_minimum=0;c.maximum=c.slider_maximum=100;c.enforce_limits=true;c.float_backed=true;c.step=.1;c.settings=runtime::ParameterSettings{false,0,100,.1};
  c.detail="0% 保留原始形变，100% 让左右鞋底前掌各自使用平均骨骼权重（包含五根脚趾及末节），并对自动体型传递保形。按静止鞋型建立连续支撑区，跨越 Foot / Toe 分区接缝，向后方和鞋面平滑过渡；高处鞋面保留原有柔性和碰撞避让。保留服装自身的调整 Morph，不改变人物姿势；硬鞋底可从 100% 开始比较。";
  c.read=std::move(read);c.write=std::move(write);return c;
}
inline ParameterControl local_shadow_control(std::function<double()> read,std::function<void(double)> write){
  ParameterControl c;c.id="render/local_shadow";c.label="局部阴影查询（实验）";c.group="/特殊处理/独立阴影结构";c.choices={"关闭","开启"};c.minimum=0;c.maximum=1;c.initial=0;
  c.detail="为此对象及其衣物、毛发、Shell、GeoGraft 启用巨型尺度阴影优化。启用的组达到巨型尺度后，所有来源的阴影射线按空间层级查询局部结构，包括太阳、天空及普通对象接收的阴影。保留双向阴影；移动相机不重建几何。普通倍率保留原路径。角色开启后附件继承，附件上的关闭不覆盖角色设置。仅 OptiX 生效。";
  c.read=std::move(read);c.write=std::move(write);return c;
}
}
