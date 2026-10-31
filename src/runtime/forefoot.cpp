#include "runtime/forefoot.h"
#include "runtime/rigid_follow.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <stdexcept>

namespace dfv::runtime {
namespace {
ir::Vec3 sub(ir::Vec3 a,ir::Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
float dot(ir::Vec3 a,ir::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float area(ir::Vec3 a,ir::Vec3 b){const ir::Vec3 n{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};return std::sqrt(dot(n,n))*.5f;}
ir::Vec3 add(ir::Vec3 a,ir::Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
ir::Vec3 mul(ir::Vec3 a,float s){return {a.x*s,a.y*s,a.z*s};}
float smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*t*(t*(t*6-15)+10);}
int toe_joint(const Skin &skin,char side){for(size_t j=0;j<skin.joints.size();++j)for(const auto *suffix:{"Toe","Toes","_toes"})if(skin.joints[j].name==std::string(1,side)+suffix)return int(j);return -1;}
bool toe_group(const std::string &group,char side){return group==std::string(1,side)+"Toe"||group==std::string(1,side)+"Toes"||group==std::string(1,side)+"_toes";}
bool foot_group(const std::string &group,char side){return toe_group(group,side)||group==std::string(1,side)+"Foot"||group==std::string(1,side)+"_foot"||group==std::string(1,side)+"Metatarsals"||group==std::string(1,side)+"_metatarsal";}
// 从下向上穿过鞋体，最厚的封闭层通常是鞋底。薄的鞋面、底纹和饰片
// 不应决定鞋底上沿；固定高度阈值会把厚底的上下表面分到不同蒙皮强度。
class SoleEnvelope {
  static constexpr int cells=32;
  const ir::Mesh &mesh_;
  ir::Bounds bounds_;
  std::array<std::vector<uint32_t>,cells*cells> grid_;
  std::array<float,cells*cells> heights_{};
  bool active_=false;
  float length_;
  int cell(float p,float low,float high) const {return std::clamp(int((p-low)/std::max(high-low,1e-7f)*cells),0,cells-1);}
  struct Layer {float top,thickness;};
  std::optional<Layer> sample(ir::Vec3 p) const {
    struct Hit {float z;int sign;};std::vector<Hit> hits;
    const int x=cell(p.x,bounds_.minimum.x,bounds_.maximum.x),y=cell(p.y,bounds_.minimum.y,bounds_.maximum.y);
    for(auto f:grid_[y*cells+x]){const auto v=mesh_.triangles[f].vertices;const auto a=mesh_.positions[v[0]],b=sub(mesh_.positions[v[1]],a),c=sub(mesh_.positions[v[2]],a),q=sub(p,a);
      const double det=double(b.x)*c.y-double(b.y)*c.x;if(std::abs(det)<1e-14)continue;
      const double u=(double(q.x)*c.y-double(q.y)*c.x)/det,w=(double(b.x)*q.y-double(b.y)*q.x)/det;
      if(u< -1e-5||w< -1e-5||u+w>1.00001)continue;hits.push_back({float(a.z+u*b.z+w*c.z),det>0?1:-1});
    }
    std::sort(hits.begin(),hits.end(),[](auto a,auto b){return a.z<b.z;});
    std::vector<Hit> crossings;for(auto h:hits){if(!crossings.empty()&&std::abs(h.z-crossings.back().z)<1e-6f){if(h.sign!=crossings.back().sign)crossings.back().sign=0;}else crossings.push_back(h);}
    float bottom=0,thickness=length_*.003f;std::optional<Layer> layer;int winding=0;
    for(auto h:crossings){const int next=std::max(0,winding-h.sign);if(winding==0&&next>0)bottom=h.z;
      if(winding>0&&next==0){const float span=h.z-bottom;if(span>thickness&&span<length_*.3f){thickness=span;layer=Layer{h.z,span};}}winding=next;}
    return layer;
  }
public:
  SoleEnvelope(const ir::Mesh &mesh,char side,float length,float full_top):mesh_(mesh),length_(length){
    for(const auto &f:mesh.triangles)if(f.polygon_group<mesh.polygon_groups.size()&&foot_group(mesh.polygon_groups[f.polygon_group],side))for(auto v:f.vertices)bounds_.add(mesh.positions[v]);
    for(uint32_t i=0;i<mesh.triangles.size();++i){const auto &f=mesh.triangles[i];if(f.polygon_group>=mesh.polygon_groups.size()||!foot_group(mesh.polygon_groups[f.polygon_group],side))continue;ir::Bounds b;for(auto v:f.vertices)b.add(mesh.positions[v]);
      const int x0=cell(b.minimum.x,bounds_.minimum.x,bounds_.maximum.x),x1=cell(b.maximum.x,bounds_.minimum.x,bounds_.maximum.x),y0=cell(b.minimum.y,bounds_.minimum.y,bounds_.maximum.y),y1=cell(b.maximum.y,bounds_.minimum.y,bounds_.maximum.y);
      for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x)grid_[y*cells+x].push_back(i);
    }
    std::array<std::optional<Layer>,cells*cells> layers;std::vector<float> thicknesses;const auto extent=sub(bounds_.maximum,bounds_.minimum);
    for(int y=0;y<cells;++y)for(int x=0;x<cells;++x){auto &layer=layers[y*cells+x];layer=sample({bounds_.minimum.x+extent.x*(x+.5f)/cells,bounds_.minimum.y+extent.y*(y+.5f)/cells,0});if(layer)thicknesses.push_back(layer->thickness);}
    if(thicknesses.empty())return;const auto middle=thicknesses.begin()+thicknesses.size()/2;std::nth_element(thicknesses.begin(),middle,thicknesses.end());const float typical=*middle;
    std::vector<int> valid;for(int i=0;i<cells*cells;++i)if(layers[i]&&layers[i]->thickness>=typical*.35f&&layers[i]->thickness<=typical*2.5f){heights_[i]=layers[i]->top;valid.push_back(i);}
    if(valid.empty())return;
    // 原范围已覆盖主体鞋底时保留既有过渡，不缩窄薄底鞋的保护范围。
    std::vector<float> tops;for(auto i:valid)tops.push_back(heights_[i]);const auto median_top=tops.begin()+tops.size()/2;
    std::nth_element(tops.begin(),median_top,tops.end());if(*median_top<=full_top)return;
    // 在统一的空间网格上补齐倒角和拼片缝隙，再做连续插值。
    // 逐顶点射线会在鞋垫投影边缘突然命中鞋面，制造孤立硬斑。
    for(int i=0;i<cells*cells;++i)if(std::find(valid.begin(),valid.end(),i)==valid.end()){float best=INFINITY;int nearest=valid.front();for(auto n:valid){const float dx=(i%cells-n%cells)*extent.x,dy=(i/cells-n/cells)*extent.y,d=dx*dx+dy*dy;if(d<best){best=d;nearest=n;}}heights_[i]=heights_[nearest];}
    for(int iteration=0;iteration<2;++iteration){auto filtered=heights_;for(int y=0;y<cells;++y)for(int x=0;x<cells;++x){std::array<float,9> values;size_t n=0;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)values[n++]=heights_[std::clamp(y+dy,0,cells-1)*cells+std::clamp(x+dx,0,cells-1)];std::nth_element(values.begin(),values.begin()+4,values.end());filtered[y*cells+x]=values[4];}heights_=filtered;}
    active_=true;
  }
  std::optional<float> top(ir::Vec3 p) const {
    if(!active_)return {};const auto e=sub(bounds_.maximum,bounds_.minimum);
    const float x=std::clamp((p.x-bounds_.minimum.x)/std::max(e.x,1e-7f)*cells-.5f,0.f,float(cells-1)),y=std::clamp((p.y-bounds_.minimum.y)/std::max(e.y,1e-7f)*cells-.5f,0.f,float(cells-1));
    const int i=std::min(int(x),cells-2),j=std::min(int(y),cells-2);const float u=x-i,v=y-j;
    return (1-v)*((1-u)*heights_[j*cells+i]+u*heights_[j*cells+i+1])+v*((1-u)*heights_[(j+1)*cells+i]+u*heights_[(j+1)*cells+i+1]);
  }
};
}
std::vector<ForefootRegion> forefoot_regions(const ir::Mesh &mesh){
  std::vector<ForefootRegion> result;
  for(char side:{'l','r'}){
    std::vector<uint8_t> foot(mesh.positions.size()),toe(mesh.positions.size());
    for(const auto &f:mesh.triangles)if(f.polygon_group<mesh.polygon_groups.size()){
      const auto &name=mesh.polygon_groups[f.polygon_group];for(auto v:f.vertices){if(foot_group(name,side))foot[v]=1;if(toe_group(name,side))toe[v]=1;}}
    ir::Vec3 front,back;float nf=0,nb=0;
    for(size_t v=0;v<foot.size();++v)if(foot[v]){if(toe[v]){front=add(front,mesh.positions[v]);++nf;}else{back=add(back,mesh.positions[v]);++nb;}}
    if(nf==0)continue;ir::Vec3 axis;
    if(nb>0)axis=sub(mul(front,1/nf),mul(back,1/nb));
    else {ir::Bounds bounds;for(size_t v=0;v<toe.size();++v)if(toe[v])bounds.add(mesh.positions[v]);const auto extent=sub(bounds.maximum,bounds.minimum);axis=extent.x>extent.y?ir::Vec3{1,0,0}:ir::Vec3{0,1,0};}
    axis.z=0;const float norm=std::sqrt(dot(axis,axis));if(norm<1e-7f)continue;axis=mul(axis,1/norm);
    float begin=INFINITY,end=-INFINITY,heel=INFINITY,bottom=INFINITY,top=-INFINITY;
    for(size_t v=0;v<foot.size();++v)if(foot[v]){const auto p=mesh.positions[v];const float x=dot(p,axis);heel=std::min(heel,x);end=std::max(end,x);if(toe[v]){begin=std::min(begin,x);bottom=std::min(bottom,p.z);top=std::max(top,p.z);}}
    const float length=end-heel,width=std::max(length*.4f,1e-6f),height=std::max(length*.08f,1e-6f);
    const SoleEnvelope sole(mesh,side,length,bottom+height);
    ForefootRegion region;region.side=side;region.blend.resize(foot.size());region.mass.resize(foot.size());
    for(size_t v=0;v<foot.size();++v)if(foot[v]){
      const auto p=mesh.positions[v];const float longitudinal=smooth(1+(dot(p,axis)-begin)/width);
      // 鞋面保持柔性，鞋底上沿以下完整保形；不能把 Foot / Toe 面分区当成鞋底接缝。
      float vertical=top-bottom<height?1:1-smooth((p.z-bottom-height)/(height*.5f));
      if(const auto upper=sole.top(p))vertical=std::max(vertical,1-smooth((p.z-*upper-length*.008f)/(height*.5f)));
      region.blend[v]=longitudinal*vertical;
    }
    for(const auto &f:mesh.triangles)if(f.polygon_group<mesh.polygon_groups.size()&&toe_group(mesh.polygon_groups[f.polygon_group],side)){
      const auto v=f.vertices;const float weight=area(sub(mesh.positions[v[1]],mesh.positions[v[0]]),sub(mesh.positions[v[2]],mesh.positions[v[0]]))/3;
      for(auto k:v)if(region.blend[k]>.9999f)region.mass[k]+=weight;
    }
    if(std::any_of(region.mass.begin(),region.mass.end(),[](float x){return x>0;}))result.push_back(std::move(region));
  }
  return result;
}
void constrain_forefoot(const std::vector<ForefootRegion> &regions,const std::vector<ir::Vec3> &reference,std::vector<ir::Vec3> &offsets,float strength){
  if(strength==0)return;
  for(const auto &region:regions){
    if(reference.size()!=region.blend.size()||offsets.size()!=reference.size())throw std::runtime_error("前掌保形顶点数量不一致");
    std::vector<ir::Vec3> a,b;for(size_t v=0;v<reference.size();++v)if(region.mass[v]>0){a.push_back(reference[v]);b.push_back(add(reference[v],offsets[v]));}
    if(a.empty())continue;auto fit=fit_rigid(a,b);ir::Vec3 ac,bc;const float count=float(a.size());
    for(size_t i=0;i<a.size();++i){ac=add(ac,mul(a[i],1/count));bc=add(bc,mul(b[i],1/count));}
    double numerator=0,denominator=0;const auto origin=fit.point({});
    for(size_t i=0;i<a.size();++i){const auto x=sub(a[i],ac),y=sub(b[i],bc);numerator+=dot(sub(fit.point(x),origin),y);denominator+=dot(x,x);}
    const float scale=denominator>1e-20&&numerator>0?float(numerator/denominator):1;
    for(int r=0;r<3;++r){for(int c=0;c<3;++c)fit.value[r*4+c]*=scale;fit.value[r*4+3]=0;}
    const auto translation=sub(bc,fit.point(ac));fit.value[3]=translation.x;fit.value[7]=translation.y;fit.value[11]=translation.z;
    for(size_t v=0;v<offsets.size();++v)if(region.blend[v]>0)offsets[v]=add(offsets[v],mul(sub(sub(fit.point(reference[v]),reference[v]),offsets[v]),strength*region.blend[v]));
  }
}
bool supports_forefoot(const ir::Mesh &mesh,const Skin &skin){
  if(mesh.positions.size()!=skin.weights.size())return false;
  for(char side:{'l','r'})if(toe_joint(skin,side)>=0)for(const auto &f:mesh.triangles)if(f.polygon_group<mesh.polygon_groups.size()&&toe_group(mesh.polygon_groups[f.polygon_group],side))return true;
  return false;
}
ForefootWeights::ForefootWeights(const ir::Mesh &mesh,const Skin &skin){
  if(!supports_forefoot(mesh,skin))return;
  for(const auto &shape:forefoot_regions(mesh)){if(toe_joint(skin,shape.side)<0)continue;const auto &masses=shape.mass;
    double total=0;std::map<uint32_t,double> mean;
    for(size_t v=0;v<masses.size();++v)if(masses[v]>0){double sum=0;for(auto w:skin.weights[v])sum+=w.weight;if(sum<=0)continue;total+=masses[v];for(auto w:skin.weights[v])mean[w.joint]+=masses[v]*w.weight/sum;}
    if(total<=0)continue;Region region;for(auto [j,w]:mean)if(w>0)region.mean.push_back({j,w/total});region.blend=shape.blend;
    regions_.push_back(std::move(region));
  }
}
Skin ForefootWeights::apply(const Skin &skin,float strength) const {
  if(!std::isfinite(strength)||strength<0||strength>1)throw std::runtime_error("前掌统一权重必须在 0–100% 之间");
  auto result=skin;if(strength==0)return result;
  for(const auto &region:regions_)for(size_t v=0;v<skin.weights.size();++v){const double blend=region.blend[v]*strength;if(blend==0)continue;std::map<uint32_t,double> weights;double total=0;for(auto w:skin.weights[v])total+=w.weight;if(total<=0)continue;
    for(auto w:skin.weights[v])weights[w.joint]+=(1-blend)*w.weight/total;for(auto w:region.mean)weights[w.joint]+=blend*w.weight;auto &out=result.weights[v];out.clear();for(auto [j,w]:weights)if(w>0)out.push_back({j,w});}
  return result;
}
}
