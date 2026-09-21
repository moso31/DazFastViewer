#pragma once
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace dfv::runtime {
enum class Op {constant,channel,knot,add,sub,mult,div,inv,neg,spline_linear,spline_constant,spline_tcb};
struct Knot {double x=0,y=0,tension=0,continuity=0,bias=0;};
struct Operation {Op code=Op::constant;double value=0;uint32_t index=0,count=0;};
struct Expression {
  uint32_t output=0,owner=0;
  bool multiply=false,enabled=true;
  int linear_input=-1;
  double coefficient=1;
  std::vector<Operation> code;
  std::vector<Knot> knots;
  std::vector<uint32_t> inputs() const;
};
enum class Property {morph,translation,rotation,scale,general_scale,center,end,orientation};
struct Binding {Property property=Property::morph;uint32_t index=0,axis=0;};
struct Channel {
  Binding binding;
  double initial=0,minimum=0,maximum=1;
  bool clamped=false,integer=false;
  std::string error;
};
struct FormulaGraph {
  std::vector<Channel> channels;
  std::vector<Expression> expressions;
  std::vector<int> morph_channels;
  int skin=-1;
  std::vector<std::vector<uint32_t>> incoming,outgoing;
  std::vector<uint32_t> order,rank;
  // 成环通道局部禁用，不能使一个第三方坏依赖拖垮整个角色。
  void prepare();
};
double sample_spline(Op kind,const Knot *knots,size_t count,double x);
double evaluate_expression(const Expression &expression,const std::vector<double> &values);
struct FormulaStats {uint64_t expressions=0,channels=0;};
class FormulaRuntime {
  const FormulaGraph &graph_;
  std::vector<double> inputs_,values_,results_;
  std::vector<bool> dirty_expressions_;
  std::set<std::pair<uint32_t,uint32_t>> dirty_;
  FormulaStats stats_;
public:
  explicit FormulaRuntime(const FormulaGraph &graph);
  bool set(uint32_t channel,double value);
  void evaluate();
  const auto &values() const {return values_;}
  const auto &stats() const {return stats_;}
};
}
