#include "runtime/formula.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <stdexcept>

namespace dfv::runtime {
std::vector<uint32_t> Expression::inputs() const {
  std::vector<uint32_t> out;if(linear_input>=0) out.push_back(uint32_t(linear_input));
  for(const auto &op:code) if(op.code==Op::channel) out.push_back(op.index);
  std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());return out;
}
double sample_spline(Op kind,const Knot *k,size_t n,double x) {
  if(n<2) throw std::runtime_error("样条至少需要两个控制点");
  if(x<=k[0].x) return k[0].y;if(x>=k[n-1].x) return k[n-1].y;
  size_t a=0;while(a+1<n&&x>=k[a+1].x) ++a;const size_t b=a+1;
  const double span=k[b].x-k[a].x,u=(x-k[a].x)/span;
  if(kind==Op::spline_constant) return k[a].y;
  if(kind==Op::spline_linear) return k[a].y+(k[b].y-k[a].y)*u;
  const double slope=(k[b].y-k[a].y)/span;
  const double previous=a?(k[a].y-k[a-1].y)/(k[a].x-k[a-1].x):slope;
  const double next=b+1<n?(k[b+1].y-k[b].y)/(k[b+1].x-k[b].x):slope;
  const auto &l=k[a];const auto &r=k[b];
  const double left=.5*(1-l.tension)*((1+l.continuity)*(1+l.bias)*previous+(1-l.continuity)*(1-l.bias)*slope);
  const double right=.5*(1-r.tension)*((1-r.continuity)*(1+r.bias)*slope+(1+r.continuity)*(1-r.bias)*next);
  return (2*u*u*u-3*u*u+1)*l.y+(u*u*u-2*u*u+u)*span*left+(-2*u*u*u+3*u*u)*r.y+(u*u*u-u*u)*span*right;
}
double evaluate_expression(const Expression &e,const std::vector<double> &values) {
  if(e.linear_input>=0) return values.at(size_t(e.linear_input))*e.coefficient;
  std::vector<double> stack;stack.reserve(e.code.size());
  auto pop=[&] {if(stack.empty()) throw std::runtime_error("公式栈下溢");const double v=stack.back();stack.pop_back();return v;};
  for(const auto &op:e.code) {
    double a,b;
    switch(op.code) {
      case Op::constant: stack.push_back(op.value);break;
      case Op::channel: stack.push_back(values.at(op.index));break;
      case Op::add: b=pop();a=pop();stack.push_back(a+b);break;
      case Op::sub: b=pop();a=pop();stack.push_back(a-b);break;
      case Op::mult: b=pop();a=pop();stack.push_back(a*b);break;
      case Op::div: b=pop();a=pop();if(b==0) throw std::runtime_error("公式除数为零");stack.push_back(a/b);break;
      case Op::inv: a=pop();if(a==0) throw std::runtime_error("公式倒数为零");stack.push_back(1/a);break;
      case Op::neg: stack.push_back(-pop());break;
      default: a=pop();if(size_t(op.index)+op.count>e.knots.size()) throw std::runtime_error("样条范围越界");stack.push_back(sample_spline(op.code,e.knots.data()+op.index,op.count,a));break;
    }
  }
  if(stack.size()!=1||!std::isfinite(stack.back())) throw std::runtime_error("公式没有产生唯一有限结果");return stack.back();
}
void FormulaGraph::prepare() {
  const size_t n=channels.size();std::vector<std::vector<uint32_t>> edges(n);
  for(const auto &e:expressions) if(e.enabled) {if(e.output>=n) throw std::runtime_error("公式输出通道越界");for(auto input:e.inputs()) {if(input>=n) throw std::runtime_error("公式输入通道越界");edges[input].push_back(e.output);}}
  for(auto &edge:edges) {std::sort(edge.begin(),edge.end());edge.erase(std::unique(edge.begin(),edge.end()),edge.end());}
  std::vector<int> index(n,-1),low(n);std::vector<uint32_t> stack;std::vector<bool> active(n);int serial=0;
  std::function<void(uint32_t)> visit=[&](uint32_t v) {
    index[v]=low[v]=serial++;stack.push_back(v);active[v]=true;
    for(auto next:edges[v]) {if(index[next]<0) {visit(next);low[v]=std::min(low[v],low[next]);}else if(active[next]) low[v]=std::min(low[v],index[next]);}
    if(low[v]==index[v]) {std::vector<uint32_t> component;uint32_t w;do {w=stack.back();stack.pop_back();active[w]=false;component.push_back(w);} while(w!=v);
      if(component.size()>1||std::binary_search(edges[v].begin(),edges[v].end(),v)) for(auto c:component) channels[c].error="公式依赖形成循环";}
  };
  for(uint32_t i=0;i<n;++i) if(index[i]<0) visit(i);
  incoming.assign(n,{});outgoing.assign(n,{});std::vector<uint32_t> degree(n);
  for(uint32_t f=0;f<expressions.size();++f) {auto &e=expressions[f];if(!channels[e.output].error.empty()) e.enabled=false;if(!e.enabled) continue;
    incoming[e.output].push_back(f);for(auto input:e.inputs()) if(channels[input].error.empty()) {outgoing[input].push_back(f);++degree[e.output];}}
  std::queue<uint32_t> ready;for(uint32_t c=0;c<n;++c) if(!degree[c]) ready.push(c);order.clear();rank.resize(n);
  while(!ready.empty()) {const auto c=ready.front();ready.pop();rank[c]=uint32_t(order.size());order.push_back(c);for(auto f:outgoing[c]) if(--degree[expressions[f].output]==0) ready.push(expressions[f].output);}
  if(order.size()!=n) throw std::runtime_error("公式循环隔离失败");
}
FormulaRuntime::FormulaRuntime(const FormulaGraph &graph):graph_(graph) {
  for(const auto &c:graph.channels) {inputs_.push_back(c.initial);values_.push_back(0);}
  results_.resize(graph.expressions.size());dirty_expressions_.assign(results_.size(),true);
  for(uint32_t c=0;c<graph.channels.size();++c) dirty_.insert({graph.rank.at(c),c});
}
bool FormulaRuntime::set(uint32_t c,double value) {
  if(!std::isfinite(value)) throw std::runtime_error("公式输入必须为有限数");const auto &channel=graph_.channels.at(c);
  if(!channel.error.empty()) value=0;if(channel.integer) value=std::round(value);if(channel.clamped) value=std::clamp(value,channel.minimum,channel.maximum);
  if(inputs_.at(c)==value) return false;inputs_[c]=value;dirty_.insert({graph_.rank[c],c});return true;
}
void FormulaRuntime::evaluate() {
  while(!dirty_.empty()) {
    const auto c=dirty_.begin()->second;dirty_.erase(dirty_.begin());const auto &channel=graph_.channels[c];double sum=inputs_[c],product=1;
    if(channel.error.empty()) for(auto f:graph_.incoming[c]) {const auto &e=graph_.expressions[f];if(dirty_expressions_[f]) {results_[f]=evaluate_expression(e,values_);dirty_expressions_[f]=false;++stats_.expressions;}
      if(e.multiply) product*=results_[f];else sum+=results_[f];}
    double result=channel.error.empty()?sum*product:0;if(!std::isfinite(result)) throw std::runtime_error("公式合并结果非有限");
    if(channel.integer) result=std::round(result);if(channel.clamped) result=std::clamp(result,channel.minimum,channel.maximum);++stats_.channels;
    if(result==values_[c]) continue;values_[c]=result;
    for(auto f:graph_.outgoing[c]) {dirty_expressions_[f]=true;const auto output=graph_.expressions[f].output;dirty_.insert({graph_.rank[output],output});}
  }
}
}
