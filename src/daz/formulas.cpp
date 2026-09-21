#include "daz/formulas.h"
#include "daz/morphs.h"
#include "daz/skeleton.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace dfv::daz {
using namespace runtime;
using Json=nlohmann::json;
uint32_t FormulaSource::symbol(const std::string &address) {
  auto [it,inserted]=interned.emplace(address,uint32_t(symbols.size()));if(inserted) symbols.push_back(address);return it->second;
}
void append_formulas(FormulaSource &source,uint32_t owner,const Json &formulas,const std::function<std::string(const std::string &)> &address) {
  const std::map<std::string,Op> operators={{"add",Op::add},{"sub",Op::sub},{"mult",Op::mult},{"div",Op::div},{"inv",Op::inv},{"neg",Op::neg},
    {"spline_linear",Op::spline_linear},{"spline_constant",Op::spline_constant},{"spline_tcb",Op::spline_tcb}};
  for(const auto &f:formulas) {
    Expression e;e.owner=owner;e.output=source.symbol(address(f.at("output").get<std::string>()));const auto stage=f.value("stage","sum");
    if(stage!="sum"&&stage!="mult"&&stage!="multiply") throw std::runtime_error("未知合并阶段："+stage);e.multiply=stage!="sum";
    for(const auto &raw:f.at("operations")) {
      const auto name=raw.at("op").get<std::string>();Operation op;
      if(name=="push") {
        if(raw.contains("url")) {op.code=Op::channel;op.index=source.symbol(address(raw.at("url").get<std::string>()));}
        else if(raw.at("val").is_number()||raw.at("val").is_boolean()) {op.value=raw.at("val").is_boolean()?(raw.at("val").get<bool>()?1:0):raw.at("val").get<double>();if(!std::isfinite(op.value)) throw std::runtime_error("公式常量非有限");}
        else {const auto &v=raw.at("val");if(!v.is_array()||(v.size()!=2&&v.size()!=5)) throw std::runtime_error("无效样条控制点");Knot k;k.x=v[0];k.y=v[1];if(v.size()==5) {k.tension=v[2];k.continuity=v[3];k.bias=v[4];}
          for(auto value:{k.x,k.y,k.tension,k.continuity,k.bias}) if(!std::isfinite(value)) throw std::runtime_error("样条常量非有限");op.code=Op::knot;op.index=uint32_t(e.knots.size());e.knots.push_back(k);}
      } else {
        if(!operators.contains(name)) throw std::runtime_error("未知运算符："+name);op.code=operators.at(name);
        if(name.starts_with("spline_")) {
          if(e.code.empty()||e.code.back().code!=Op::constant) throw std::runtime_error("样条缺少常量控制点数量");
          const double count=e.code.back().value;e.code.pop_back();if(count<2||count>e.code.size()||count!=std::floor(count)) throw std::runtime_error("样条控制点数量无效");op.count=uint32_t(count);
          const size_t first=e.code.size()-op.count;op.index=e.code[first].index;
          for(size_t i=first;i<e.code.size();++i) if(e.code[i].code!=Op::knot||e.code[i].index!=op.index+i-first) throw std::runtime_error("样条控制点必须按顺序直接压栈");
          for(uint32_t i=1;i<op.count;++i) if(e.knots[op.index+i].x<=e.knots[op.index+i-1].x) throw std::runtime_error("样条关键值必须严格递增");e.code.resize(first);
        }
      }
      e.code.push_back(op);
    }
    int depth=0;for(const auto &op:e.code) {
      if(op.code==Op::constant||op.code==Op::channel) ++depth;
      else if(op.code==Op::knot) throw std::runtime_error("样条控制点没有被消费");
      else if(op.code==Op::add||op.code==Op::sub||op.code==Op::mult||op.code==Op::div) {if(depth<2) throw std::runtime_error("公式栈下溢");--depth;}
      else if(depth<1) throw std::runtime_error("公式栈下溢");
    }
    if(depth!=1) throw std::runtime_error("公式须产生一个标量结果");
    if(e.code.size()==1&&e.code[0].code==Op::channel) e.linear_input=int(e.code[0].index);
    if(e.code.size()==2&&e.code[0].code==Op::channel&&e.code[1].code==Op::neg) {e.linear_input=int(e.code[0].index);e.coefficient=-1;}
    if(e.code.size()==3&&e.code[2].code==Op::mult) {
      if(e.code[0].code==Op::channel&&e.code[1].code==Op::constant) {e.linear_input=int(e.code[0].index);e.coefficient=e.code[1].value;}
      if(e.code[1].code==Op::channel&&e.code[0].code==Op::constant) {e.linear_input=int(e.code[1].index);e.coefficient=e.code[0].value;}
    }
    if(e.linear_input>=0) e.code.clear();source.expressions.push_back(std::move(e));
  }
}
FormulaCatalog enable_formulas(MorphCatalog &catalog,const SkinCatalog &skins) {
  FormulaCatalog result;result.report={{"targets",Json::array()}};
  for(size_t t=0;t<catalog.targets.size();++t) {
    auto &target=catalog.targets[t];auto &source=catalog.formulas.at(t);FormulaGraph graph;
    std::map<std::string,int> assets,nodes;std::map<std::string,std::vector<int>> ids;
    std::vector<std::string> failures(target.morphs.size());
    for(size_t m=0;m<target.morphs.size();++m) {
      const auto &p=target.morphs[m];Channel c;c.binding.index=uint32_t(m);c.initial=p.initial;c.minimum=p.minimum;c.maximum=p.maximum;c.clamped=p.clamped;c.integer=p.value_type=="bool";
      c.error=p.intrinsic_error;failures[m]=c.error;graph.channels.push_back(c);graph.morph_channels.push_back(int(m));assets[p.id]=int(m);if(p.kind!="alias") ids[p.channel_id].push_back(int(m));
    }
    for(size_t s=0;s<skins.skins.size();++s) if(skins.skins[s].instance==target.instance) graph.skin=int(s);
    auto add=[&](const std::string &key,Binding binding,double initial) {Channel c;c.binding=binding;c.initial=initial;const auto id=int(graph.channels.size());graph.channels.push_back(c);nodes[key]=id;};
    if(graph.skin>=0) {
      const auto &skin=skins.skins[size_t(graph.skin)];
      for(size_t j=0;j<skin.joints.size();++j) {
        const auto &bone=skin.joints[j];const auto &pose=skin.initial[j];const std::string prefix="$node/"+bone.id+"?";
        const std::array<std::pair<Property,ir::Vec3>,6> fields={{{Property::translation,pose.translation_cm},{Property::rotation,pose.rotation_degrees},{Property::scale,pose.scale},
          {Property::center,bone.center_cm},{Property::end,bone.end_cm},{Property::orientation,bone.orientation_degrees}}};
        const char *names[]={"translation","rotation","scale","center_point","end_point","orientation"};
        for(size_t f=0;f<fields.size();++f) {const auto p=fields[f].second;const double xyz[]={p.x,p.y,p.z};for(uint32_t axis=0;axis<3;++axis) add(prefix+names[f]+"/"+char('x'+axis),{fields[f].first,uint32_t(j),axis},xyz[axis]);}
        add(prefix+"scale/general",{Property::general_scale,uint32_t(j),0},pose.general_scale);nodes[prefix+"general_scale"]=nodes.at(prefix+"scale/general");
      }
      for(const auto &root:source.roots) if(root!=skin.joints[0].id) {
        const auto prefix="$node/"+skin.joints[0].id+"?";std::vector<std::pair<std::string,int>> aliases;for(const auto &[key,value]:nodes) if(key.starts_with(prefix)) aliases.push_back({"$node/"+root+"?"+key.substr(prefix.size()),value});for(const auto &[key,value]:aliases) nodes[key]=value;
      }
    }
    Json node_failures=Json::array();size_t node_formula_count=0;
    if(graph.skin>=0&&size_t(graph.skin)<skins.node_formulas.size()) {
      auto node_address=[](const std::string &uri) {
        const auto decoded=decode_uri(uri);const auto hash=decoded.find('#'),query=decoded.find('?');
        if(hash==std::string::npos||query==std::string::npos||hash>=query) return "$invalid-node/"+decoded;
        // 基础 DSF 节点公式仅允许当前骨架的局部节点引用。
        const auto prefix=decoded.substr(0,hash);
        if(prefix.find('/')!=std::string::npos||prefix.find('\\')!=std::string::npos) return "$external-node/"+decoded;
        return "$node/"+decoded.substr(hash+1);
      };
      for(const auto &f:skins.node_formulas[size_t(graph.skin)]) {
        try {append_formulas(source,UINT32_MAX,Json::array({f}),node_address);++node_formula_count;}
        catch(const std::exception &e) {node_failures.push_back({{"output",f.value("output","")},{"reason",e.what()}});}
      }
    }
    auto resolve=[&](std::string address)->int {
      if(address.ends_with("/value")) address.resize(address.size()-6);
      if(nodes.contains(address)) return nodes.at(address);
      const bool local=address.starts_with("$local/");if(local) address.erase(0,7);
      const auto query=address.rfind('?');if(query==std::string::npos||address.substr(query+1)!="value") return -1;address.resize(query);
      if(assets.contains(address)) return assets.at(address);
      if(local) {const auto hash=address.rfind('#');const auto id=address.substr(hash+1);if(ids.contains(id)&&ids.at(id).size()==1) return ids.at(id)[0];}
      return -1;
    };
    std::vector<int> alias_state(target.morphs.size());
    std::function<int(size_t)> alias=[&](size_t m)->int {
      if(target.morphs[m].kind!="alias") return int(m);
      if(alias_state[m]==2) return graph.morph_channels[m];
      if(alias_state[m]==1) {failures[m]="参数别名形成循环";return -1;}alias_state[m]=1;
      int index=resolve(source.alias_symbols.at(m));if(index>=0&&size_t(index)<target.morphs.size()) index=alias(size_t(index));
      if(index<0) failures[m]="别名目标缺失、被本代覆盖或不能唯一解析";
      graph.morph_channels[m]=index;alias_state[m]=2;return index;
    };
    for(size_t m=0;m<target.morphs.size();++m) alias(m);
    std::vector<int> symbols;for(const auto &address:source.symbols) {int c=resolve(address);if(c>=0&&size_t(c)<target.morphs.size()) c=graph.morph_channels[size_t(c)];symbols.push_back(c);}
    size_t unresolved=0;
    for(auto &raw:source.expressions) {
      auto e=std::move(raw);auto missing=e.output;const int output=symbols.at(e.output);bool valid=output>=0;
      for(const auto input:e.inputs()) if(symbols.at(input)<0) {valid=false;missing=input;}
      if(!valid) {++unresolved;const auto reason="公式引用缺失、被本代覆盖或不支持的属性："+source.symbols[missing];
        if(e.owner<failures.size()) failures[e.owner]=reason;else node_failures.push_back({{"output",source.symbols[e.output]},{"reason",reason}});continue;}
      e.output=uint32_t(output);if(e.linear_input>=0) e.linear_input=symbols[size_t(e.linear_input)];for(auto &op:e.code) if(op.code==Op::channel) op.index=uint32_t(symbols[op.index]);graph.expressions.push_back(std::move(e));
    }
    for(size_t m=0;m<target.morphs.size();++m) graph.channels[m].error=failures[m];
    for(auto &e:graph.expressions) if(e.owner<failures.size()&&!failures[e.owner].empty()) e.enabled=false;
    graph.prepare();size_t editable=0,visible=0,aliases=0,cycles=0;
    for(const auto &c:graph.channels) if(c.error=="公式依赖形成循环") ++cycles;
    for(size_t m=0;m<target.morphs.size();++m) {
      auto &p=target.morphs[m];const auto index=graph.morph_channels[m];std::string error=failures[m];
      if(index<0) {if(error.empty()) error="参数没有可求值的目标";}
      else {const auto &c=graph.channels[size_t(index)];if(error.empty()) error=c.error;
        if(p.kind=="alias"&&error.empty()) {p.minimum=float(c.minimum);p.maximum=float(c.maximum);p.clamped=c.clamped;p.initial=float(c.initial);if(c.binding.property==Property::morph) {p.alias_morph=int(c.binding.index);p.step=target.morphs[c.binding.index].step;p.locked=target.morphs[c.binding.index].locked;p.value_type=target.morphs[c.binding.index].value_type;}else error="当前参数面板尚未支持指向骨骼属性的别名";++aliases;}
        if(error.empty()&&p.kind!="alias"&&p.offsets.empty()&&graph.incoming[m].empty()&&graph.outgoing[m].empty()) error="没有顶点差值或可求值的依赖输出";
      }
      p.evaluable=error.empty();p.unsupported=error.empty()&&p.locked?"资产将此参数标为锁定":error;
      if(p.unsupported.empty()) {++editable;if(p.visible) ++visible;}
      auto &item=catalog.report["targets"][t]["morphs"][m];item["unsupported"]=p.unsupported;item["evaluable"]=p.evaluable;item["alias_morph"]=p.alias_morph;
      item["initial"]=p.initial;item["min"]=p.minimum;item["max"]=p.maximum;
    }
    size_t enabled=0;for(const auto &e:graph.expressions) enabled+=e.enabled;
    result.report["targets"].push_back({{"id",target.id},{"channels",graph.channels.size()},{"formulas",graph.expressions.size()},{"enabled_formulas",enabled},{"unresolved_formulas",unresolved},
      {"cyclic_channels",cycles},{"editable_parameters",editable},{"visible_editable_parameters",visible},{"resolved_aliases",aliases},
      {"node_formulas",node_formula_count},{"node_formula_failures",node_failures}});
    source={};result.graphs.push_back(std::move(graph));
  }
  return result;
}
}
