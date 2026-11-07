#include "runtime/growth_height.h"
static void growth_curve_checks(){
  using namespace runtime;
  GrowthCurve c;near(growth_curve_scale(c,0),30,1e-12,"出生端点错误");near(growth_curve_scale(c,18),100,1e-12,"默认成年端点错误");
  for(int kind=0;kind<3;++kind){apply_growth_template(c,kind);c.minimum_growth=3;fit_growth_curve(c);near(growth_curve_scale(c,18),100,1e-10,"B1 未保持成年目标");
    double last=0;for(int i=0;i<=1800;++i){const double age=i*.01,s=growth_curve_scale(c,age);require(s>=last,"曲线发生倒退");last=s;if(age<18)require(growth_curve_rate(c,age)>=3,"连续增长低于 B");}
    const auto a=c.shape.points[5].age;require(growth_curve_rate(c,a)>=growth_curve_rate(c,a-.01)&&growth_curve_rate(c,a)>=growth_curve_rate(c,a+.01),"模板 A 不在青春期峰值");
  }
  c.minimum_growth=4;const auto invalid=c;bool rejected=false;try{fit_growth_curve(c);}catch(...){rejected=true;}require(rejected&&c==invalid,"B1 无解未原子拒绝");
  c.minimum_mode=GrowthMinimumMode::additive;fit_growth_curve(c);near(growth_curve_scale(c,18),172,1e-10,"B2 未叠加累计最低增长");
  near(growth_curve_scale(c,50),172,1e-10,"越界年龄继续增长");near(growth_curve_scale(c,std::numeric_limits<double>::max()),172,1e-10,"极大年龄越界");near(growth_curve_rate(c,18),0,0,"成年未停止增长");
  move_growth_peak(c,13.375);near(c.shape.points[5].age,13.375,0,"小数 A 未保存");
  const auto a=13.375;require(growth_curve_rate(c,a)>growth_curve_rate(c,a-.01)&&growth_curve_rate(c,a)>growth_curve_rate(c,a+.01),"小数 A 未移动速度峰值");
  for(double t:{.125,1.75,5.25,12.375,17.75})near((growth_curve_scale(c,t+1e-5)-growth_curve_scale(c,t-1e-5))/2e-5,growth_curve_rate(c,t),1e-7,"Scale 积分与速度不一致");
  c.birth_scale=30;c.adult_scale=102;c.minimum_growth=4;c.minimum_mode=GrowthMinimumMode::fit;fit_growth_curve(c);near(growth_curve_scale(c,7.5),60,1e-12,"B1 等号边界未退化为匀速");
  for(double bad:{-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){rejected=false;try{growth_curve_scale(c,bad);}catch(...){rejected=true;}require(rejected,"无效年龄未拒绝");}
  auto d=growth_document();auto s=editor::initial_snapshot(d);s.values[0].extension.kind=ExtensionKind::growth;s.values[0].transform.translation_cm={12,23,34};
  auto next=s.values[0].extension;next.curve.enabled=true;next.curve.minimum_growth=1;fit_growth_curve(next.curve);
  editor::edit_growth(d,s,0,next,false);const auto scale=[&]{return s.values[0].transform.general_scale*.99*100;};
  near(scale(),growth_curve_scale(next.curve,next.age),1e-9,"启用没有应用当前年龄绝对 Scale");const auto origin=s.values[0].transform.translation_cm;
  for(double age:{0.,.125,7.5,13.375,18.,30.,50.,1e30}){next.age=age;editor::edit_growth(d,s,0,next);near(scale(),growth_curve_scale(next.curve,age),1e-8,"年龄未驱动实际根 Scale");require(s.values[0].transform.translation_cm==origin,"曲线额外修改位置");}
  const auto mature=s.values[0];next.age=18;editor::edit_growth(d,s,0,next);require(s.values[0].morphs==mature.morphs,"成年后年龄形变没有冻结");
  for(int i=0;i<80;++i){next.age=.125;editor::edit_growth(d,s,0,next);next.age=18;editor::edit_growth(d,s,0,next);}
  near(s.values[0].transform.general_scale,mature.transform.general_scale,0,"曲线往返发生累计漂移");
  auto before=s;auto broken=next;broken.curve.shape.points[3].age=broken.curve.shape.points[2].age;rejected=false;try{editor::edit_growth(d,s,0,broken);}catch(...){rejected=true;}require(rejected&&s.values==before.values,"曲线无效节点产生部分场景修改");
  next.curve.enabled=false;editor::edit_growth(d,s,0,next,false);near(s.values[0].transform.general_scale,mature.transform.general_scale,0,"关闭曲线改变 Scale");next.age+=.5;editor::edit_growth(d,s,0,next);near(scale(),100+.5*next.sensitivity,1e-8,"关闭曲线后未恢复旧每年增长");
  const auto legacy=growth_values(50,.5),edge=growth_values(20,.5);for(size_t i=0;i<legacy.size();++i)near(legacy[i].value,edge[i].value,0,"旧形变表超龄访问越界");
  for(double adult:{8.,25.}){
    GrowthCurve custom;custom.minimum_growth=1;resize_growth_curve(custom,adult);fit_growth_curve(custom);
    near(custom.planned_age,adult,0,"默认横轴未跟随成年边界");near(growth_curve_scale(custom,adult),100,1e-10,"可变成年边界未拟合目标");
    near(growth_curve_scale(custom,50),100,1e-10,"可变成年边界后继续增长");require(growth_curve_rate(custom,adult-1e-6)>=1&&growth_curve_rate(custom,adult)==0,"最低增长未在成年边界停止");
    next.curve=custom;next.curve.enabled=true;next.age=50;editor::edit_growth(d,s,0,next,false);const auto frozen=s.values[0].morphs;
    next.age=custom.planned_age;editor::edit_growth(d,s,0,next);require(s.values[0].morphs==frozen,"修改成年边界没有同步当前年龄的 Morph");
    s.values[0].transform.general_scale=1.23;next.curve.height_in_cm=true;next.curve.growth_in_cm=true;
    editor::edit_growth(d,s,0,next,false);near(s.values[0].transform.general_scale,1.23,0,"图表显示参数意外修改 Scale");
  }
  c=GrowthCurve{};const double left=(growth_curve_rate(c,1)-growth_curve_rate(c,1-1e-5))/1e-5,right=(growth_curve_rate(c,1+1e-5)-growth_curve_rate(c,1))/1e-5;
  require(left<-.1&&right<-.1,"单调节点处仍强制变平");near(left,right,.001,"阶段节点切线不连续");
  const double ratio=(growth_curve_scale(c,18)-growth_curve_scale(c,8))/70;
  c.puberty_weight=3;fit_growth_curve(c);near(growth_curve_scale(c,18),100,1e-10,"青春期权重参与拟合后端点偏离");
  require((growth_curve_scale(c,18)-growth_curve_scale(c,8))/70>ratio,"提高青春期权重未增加青春期占比");
  c.puberty_weight=0;fit_growth_curve(c);near(growth_curve_scale(c,18),100,1e-10,"零青春期权重拟合失败");
  for(int i=0;i<1800;++i)require(growth_curve_rate(c,i*.01)>=0,"零权重引入负增长");
  c=GrowthCurve{};resize_growth_curve(c,25);const std::array<CurveKnot,4> heights{{{0,120},{8,140},{18,180},{25,185}}};
  for(double age:{.125,3.5,8.,13.25,24.875}){
    const double derivative=(growth_curve_height(c,heights,age+1e-5)-growth_curve_height(c,heights,age-1e-5))/2e-5;
    near(growth_curve_height_rate(c,heights,age),derivative,1e-5,"cm 每年增长未计入 Morph 身高变化");
  }
  near(growth_curve_height(c,heights,50),185,1e-9,"cm 身高成年后未冻结");near(growth_curve_height_rate(c,heights,25),0,0,"cm 增长成年后未停止");
  c=GrowthCurve{};move_growth_peak(c,13.375);c.shape.points[3].age=5.25;mark_growth_shape_custom(c);
  const auto custom_shape=c.shape;c.adult_scale=123;fit_growth_curve(c);
  require(c.shape==custom_shape,"拟合覆盖了自定义形状");near(growth_curve_scale(c,18),123,1e-9,"自定义形状没有满足端点");
  const auto fitted=c;fit_growth_curve(c);require(c==fitted,"重复拟合发生形状或倍率漂移");
  c.adult_scale=c.birth_scale;fit_growth_curve(c);require(c.shape==custom_shape&&c.growth_factor==0,"零额外增长丢失形状");
  c.adult_scale=123;fit_growth_curve(c);near(growth_curve_scale(c,18),123,1e-9,"零增长后无法基于保留形状恢复拟合");
  resize_growth_curve(c,25);require(c.shape.reference_template==GrowthCurveShape::custom_template,"规划年龄编辑未标记自定义");
  apply_growth_template(c,1);require(c.shape.reference_template==1&&c.planned_age==18,"参考模板未恢复规划年龄");near(c.shape.points[5].age,11.7,0,"参考模板未恢复峰值年龄");near(c.shape.points[3].age,5,0,"参考模板未恢复儿童期年龄");
  for(auto &p:c.shape.points)p.rate=0;const auto empty=c;rejected=false;try{fit_growth_curve(c);}catch(...){rejected=true;}require(rejected&&c==empty,"零面积形状被偷偷替换或未提示无解");

  c=GrowthCurve{};near(c.shape.points[5].age,12.5,0,"默认峰值年龄不是 12.5");near(c.shape.points[6].age,15,0,"默认减速年龄不是 15");
  auto independent=c;move_growth_peak(c,13.5);for(size_t i=0;i<c.shape.points.size();++i)if(i!=5)require(c.shape.points[i]==independent.shape.points[i],"峰值编辑联动了其他阶段");
  const auto stages=c.shape;const double multiplier=c.growth_factor;
  for(double age:{8.,25.,.5,50.}){resize_growth_curve(c,age);require(c.shape==stages&&c.growth_factor==multiplier,"规划年龄改变形状或增长倍率");fit_growth_curve(c);near(growth_curve_scale(c,age),100,1e-9,"独立规划边界拟合错误");c.growth_factor=multiplier;}
  set_growth_stage_age(c,7,22);require(c.planned_age==50&&c.shape.points[6].age==15,"成年阶段修改影响规划年龄或减速阶段");
  const auto valid=c;rejected=false;try{set_growth_stage_age(c,5,16);}catch(...){rejected=true;}require(rejected&&c==valid,"阶段顺序冲突未原子拒绝");

}
