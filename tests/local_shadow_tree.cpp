#include <cstdint>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
using uint = unsigned int;
#define CCL_NAMESPACE_BEGIN namespace ccl {
#define CCL_NAMESPACE_END }
#include "cycles/local_shadow_tree.h"

using Node=ccl::DfvLocalShadowNode;
static void check(bool v,const char *message){if(!v)throw std::runtime_error(message);}
static bool hit(const Node &n,const double *p,const double *d){
  double near=0,far=1e30;
  for(int a=0;a<3;++a){if(d[a]==0){if(p[a]<n.lower[a]||p[a]>n.upper[a])return false;}else{const double x=(n.lower[a]-p[a])/d[a],y=(n.upper[a]-p[a])/d[a];near=std::max(near,std::min(x,y));far=std::min(far,std::max(x,y));}}
  return near<=far;
}
int main(){try{
  check(ccl::dfv_build_shadow_tree({}).empty(),"Empty scene produced nodes");
  std::mt19937 random(1337);std::uniform_real_distribution<double> u(-1,1);
  std::vector<Node> leaves;
  for(int i=0;i<257;++i){Node n{};n.object=i;n.visibility=1u<<(i%3);const double scale=std::pow(10.,i%13-5);for(int a=0;a<3;++a){const double center=u(random)*scale,extent=(i%9==0?0:scale*.2);n.lower[a]=float(center-extent);n.upper[a]=float(center+extent);}leaves.push_back(n);}
  const auto nodes=ccl::dfv_build_shadow_tree(leaves);check(nodes.size()==leaves.size()*2-1,"Wrong node count");
  for(size_t i=0;i<nodes.size();++i){check(nodes[i].escape>int(i)&&nodes[i].escape<=int(nodes.size()),"Invalid escape index");if(nodes[i].object>=0)check(nodes[i].escape==int(i+1),"Leaf skips another object");}
  size_t visits=0;
  for(int ray=0;ray<3000;++ray){double p[3],d[3];const double scale=std::pow(10.,ray%13-5);for(int a=0;a<3;++a){p[a]=u(random)*scale;d[a]=(ray%7==a?0:u(random));}const uint mask=1u<<(ray%3);std::set<int> expected,actual;
    for(const auto &n:leaves)if((n.visibility&mask)&&hit(n,p,d))expected.insert(n.object);
    for(int i=0;i<int(nodes.size());){const auto &n=nodes[i];++visits;if(!(n.visibility&mask)||!hit(n,p,d)){i=n.escape;continue;}if(n.object>=0)actual.insert(n.object);++i;}
    check(actual==expected,"Hierarchy lost or duplicated a ray candidate");
  }
  check(visits<3000*nodes.size()/2,"Hierarchy did not prune spatially separated objects");
  std::cout<<"PASS: 3000 mixed-scale and axis-parallel rays, "<<visits<<" node visits\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
