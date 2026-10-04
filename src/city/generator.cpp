#include "city/generator.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <chrono>
#include <fstream>
#include <mutex>
#include <numeric>
#include <set>
#include <sstream>

namespace dfv::city {
namespace {
uint64_t mix(uint64_t x){x+=0x9e3779b97f4a7c15ull;x=(x^(x>>30))*0xbf58476d1ce4e5b9ull;x=(x^(x>>27))*0x94d049bb133111ebull;return x^(x>>31);}
double random(uint64_t x){return double(mix(x)>>11)*0x1.0p-53;}
std::string name(const std::filesystem::path &p){const auto s=p.stem().u8string();return {s.begin(),s.end()};}
void compact(ir::Mesh &mesh){std::vector<uint32_t> indices(mesh.positions.size(),UINT32_MAX);std::vector<ir::Vec3> positions;for(auto &t:mesh.triangles)for(auto &v:t.vertices){auto &index=indices.at(v);if(index==UINT32_MAX){index=uint32_t(positions.size());positions.push_back(mesh.positions[v]);}v=index;}mesh.positions=std::move(positions);}
void quad(ir::Mesh &m,std::array<ir::Vec3,4> p,uint32_t slot=0,std::array<ir::Vec2,4> uv={ir::Vec2{0,0},{1,0},{1,1},{0,1}}){
  const auto first=uint32_t(m.positions.size());m.positions.insert(m.positions.end(),p.begin(),p.end());
  for(auto corners:{std::array<int,3>{0,1,2},{0,2,3}}){ir::Triangle t;t.material_slot=slot;t.source_polygon=uint32_t(m.triangles.size());for(int k=0;k<3;++k){t.vertices[k]=first+corners[k];t.uv[k]=uv[corners[k]];}m.triangles.push_back(t);}
}
void box(ir::Mesh &m,ir::Vec3 a,ir::Vec3 b,uint32_t slot=0){
  const ir::Vec3 p000{a.x,a.y,a.z},p100{b.x,a.y,a.z},p110{b.x,b.y,a.z},p010{a.x,b.y,a.z};
  const ir::Vec3 p001{a.x,a.y,b.z},p101{b.x,a.y,b.z},p111{b.x,b.y,b.z},p011{a.x,b.y,b.z};
  quad(m,{p000,p010,p110,p100},slot);quad(m,{p001,p101,p111,p011},slot);
  quad(m,{p000,p100,p101,p001},slot);quad(m,{p100,p110,p111,p101},slot);quad(m,{p110,p010,p011,p111},slot);quad(m,{p010,p000,p001,p011},slot);
}
void append_mesh(ir::Mesh &out,const ir::Mesh &in,const ir::Transform &transform,uint32_t slot){const auto offset=uint32_t(out.positions.size());for(auto p:in.positions)out.positions.push_back(transform.point(p));for(auto t:in.triangles){for(auto &v:t.vertices)v+=offset;t.material_slot=slot;t.source_polygon=uint32_t(out.triangles.size());out.triangles.push_back(t);}}
bool detail(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));});for(const auto *word:{"aircon","vent","windowframe","windowsurround","windowstrut","architrave","antenna","flagpole","clockhands","graffiti"})if(s.find(word)!=std::string::npos)return true;return false;}
ir::Mesh coarse(const Asset &a){ir::Mesh m;m.smooth=false;m.material_slots={"Facade"};
  for(const auto *part:{"Walls","LeftWalls","RightWalls","FrontBlock","TopLevelWall","LiftBlock","Tower","Tower2"}){
    auto it=std::find(a.detailed.material_slots.begin(),a.detailed.material_slots.end(),part);if(it==a.detailed.material_slots.end())continue;ir::Bounds b;
    for(const auto &t:a.detailed.triangles)if(t.material_slot==uint32_t(it-a.detailed.material_slots.begin()))for(auto v:t.vertices)b.add(a.detailed.positions[v]);
    if(!b.empty&&b.maximum.x-b.minimum.x>.3f&&b.maximum.y-b.minimum.y>.3f&&b.maximum.z-b.minimum.z>.5f)box(m,b.minimum,b.maximum);
  }
  if(m.triangles.empty())box(m,a.bounds.minimum,a.bounds.maximum);return m;
}
uint32_t add_material(ir::Scene &s,ir::Material m){const auto i=uint32_t(s.materials.size());s.materials.push_back(std::move(m));return i;}
uint32_t prototype(ir::Scene &s,ir::Mesh mesh,std::vector<uint32_t> mats,const std::string &id,const std::string &node){
  mesh.id=id+"/mesh";const auto mi=uint32_t(s.meshes.size());s.meshes.push_back(std::move(mesh));ir::Instance i;i.id=id;i.instance_label=id;i.instance_node=node;i.mesh=mi;i.materials=std::move(mats);i.visible=false;const auto n=uint32_t(s.instances.size());s.instances.push_back(std::move(i));return n;
}
std::string instance(ir::Scene &s,uint32_t proto,const std::string &id,const std::string &root,const std::string &cell,const ir::Transform &transform,bool visible=false){
  auto i=s.instances.at(proto);i.id=id;i.prototype=int(proto);i.visible=visible;i.transform=transform;i.instance_node=root;i.instance_group=cell;i.instance_label=cell.substr(cell.rfind('/')+1);s.instances.push_back(std::move(i));return id;
}
std::filesystem::path height_image(const std::vector<float> &pixels){
  uint64_t h=1469598103934665603ull;for(float f:pixels){auto v=std::bit_cast<uint32_t>(f);for(int b=0;b<4;++b){h^=(v>>(b*8))&255;h*=1099511628211ull;}}
  std::ostringstream tag;tag<<std::hex<<h;auto directory=std::filesystem::temp_directory_path()/"DazFastViewer"/"city-cache-v1";std::filesystem::create_directories(directory);auto file=directory/(tag.str()+".pfm");
  static std::mutex mutex;std::lock_guard lock(mutex);if(std::filesystem::exists(file))return file;
  auto temporary=file;temporary+=L".tmp";try{std::ofstream stream(temporary,std::ios::binary);stream<<"Pf\n"<<pixels.size()<<" 1\n-1.0\n";stream.write(reinterpret_cast<const char *>(pixels.data()),std::streamsize(pixels.size()*sizeof(float)));stream.close();if(!stream)throw std::runtime_error("无法写入城市置换缓存");std::filesystem::rename(temporary,file);}catch(...){std::error_code ec;std::filesystem::remove(temporary,ec);throw;}return file;
}
}
Asset prepare_asset(const std::filesystem::path &file,const std::vector<std::filesystem::path> &roots){
  const auto loaded=daz::load(file,{roots,false});Asset a;a.name=name(file);a.textures=loaded.scene.textures;a.detailed.smooth=false;
  for(const auto &i:loaded.scene.instances){if(!i.visible)continue;const auto &m=loaded.scene.meshes.at(i.mesh);if(!m.curves.empty())throw std::runtime_error("建筑原型暂不支持曲线几何");const auto first=uint32_t(a.detailed.positions.size()),slot=uint32_t(a.detailed.material_slots.size());
    for(auto p:m.positions)a.detailed.positions.push_back(i.transform.point(p));
    for(size_t k=0;k<m.material_slots.size();++k){a.detailed.material_slots.push_back(m.material_slots[k]);a.materials.push_back(loaded.scene.materials.at(i.materials.at(k)));}
    for(auto t:m.triangles)if(m.draws(t)){const auto &mat=a.materials.at(slot+t.material_slot);if(mat.opacity<=0&&mat.opacity_texture<0)continue;for(auto &v:t.vertices)v+=first;t.material_slot+=slot;t.source_polygon=uint32_t(a.detailed.triangles.size());a.detailed.triangles.push_back(t);}
  }
  if(a.detailed.triangles.empty())throw std::runtime_error("建筑没有可绘制三角形："+a.name);if(a.detailed.triangles.size()>1000000)throw std::runtime_error("单栋建筑超过 100 万基础三角形："+a.name);
  compact(a.detailed);ir::Bounds raw;for(const auto &t:a.detailed.triangles)for(auto v:t.vertices)raw.add(a.detailed.positions[v]);const ir::Vec3 offset{raw.center().x,raw.center().y,raw.minimum.z};
  for(auto &p:a.detailed.positions){p={p.x-offset.x,p.y-offset.y,p.z-offset.z};a.bounds.add(p);}
  a.simple=a.detailed;std::erase_if(a.simple.triangles,[&](const auto &t){return detail(a.simple.material_slots.at(t.material_slot));});
  if(a.simple.triangles.empty())a.simple=a.detailed;compact(a.simple);return a;
}
Generated generate(Config c,const std::string &id,const std::vector<std::filesystem::path> &roots,const Progress &progress){
  validate(c);c.directory=std::filesystem::absolute(c.directory).lexically_normal();if(c.assets.empty())for(const auto &p:discover(c.directory)){auto s=p.filename().u8string();c.assets.emplace_back(s.begin(),s.end());}
  validate(c);std::sort(c.assets.begin(),c.assets.end());std::vector<Asset> assets;for(const auto &p:c.assets){if(progress)progress("读取建筑原型："+p);assets.push_back(prepare_asset(c.directory/std::filesystem::u8path(p),roots));}return generate(std::move(c),id,std::move(assets),progress);
}
Generated generate(Config c,const std::string &id,std::vector<Asset> assets,const Progress &progress){
  validate(c);if(id.empty()||assets.empty())throw std::runtime_error("城市身份和原型不能为空");Generated result;auto city=std::make_shared<City>();city->id=id;city->config=c;auto &s=result.loaded.scene;
  daz::AssetNode root;root.id=id;root.label="PCG 城市 · "+std::to_string(c.seed);root.group=true;result.loaded.nodes.push_back(root);
  daz::AssetNode catalog;catalog.id=id+"/catalog";catalog.parent="#"+id;catalog.label="城市共享材质";catalog.group=true;result.loaded.nodes.push_back(catalog);
  std::vector<std::array<uint32_t,2>> prototypes;std::vector<ir::Mesh> boxes;std::vector<uint32_t> proxy_materials;
  for(size_t index=0;index<assets.size();++index){auto &a=assets[index];const auto key=id+"/asset/"+a.name;std::vector<uint32_t> mats;const auto offset=int(s.textures.size());s.textures.insert(s.textures.end(),a.textures.begin(),a.textures.end());
    for(size_t k=0;k<a.materials.size();++k){auto m=a.materials[k];m.id=key+"/material/"+std::to_string(k);for(auto *t:ir::texture_indices(m))if(*t>=0)*t+=offset;mats.push_back(add_material(s,std::move(m)));}
    auto detailed=prototype(s,a.detailed,mats,key+"/LOD1",catalog.id);s.instances[detailed].instance_label=a.name;
    auto simple=prototype(s,a.simple,mats,key+"/LOD2",catalog.id);prototypes.push_back({detailed,simple});city->material_sources.push_back(s.instances[detailed].id);
    city->materials.push_back({s.instances[simple].id,s.instances[detailed].id});boxes.push_back(coarse(a));
    ir::Material material;material.id=key+"/proxy";material.roughness=.75f;for(size_t k=0;k<a.detailed.material_slots.size();++k)if(a.detailed.material_slots[k]=="Walls"){material.base_color=a.materials[k].base_color;break;}proxy_materials.push_back(add_material(s,material));
  }
  ir::Material road;road.id=id+"/asphalt";road.base_color={.07f,.08f,.09f};road.roughness=.9f;const auto asphalt=add_material(s,road);
  road.id=id+"/pavement";road.base_color={.28f,.27f,.25f};const auto pavement=add_material(s,road);
  const float pitch=float(c.block_size+c.road_width),half=float(c.block_size*.5),lot=float(c.block_size/c.lots);
  const ir::Transform origin=ir::Transform::translate({float(c.origin_x),float(c.origin_y),float(c.origin_z)});
  ir::Mesh ground;ground.smooth=false;ground.material_slots={"Asphalt","Pavement"};quad(ground,{{{-pitch*.5f,-pitch*.5f,0},{pitch*.5f,-pitch*.5f,0},{pitch*.5f,pitch*.5f,0},{-pitch*.5f,pitch*.5f,0}}});quad(ground,{{{-half,-half,.12f},{half,-half,.12f},{half,half,.12f},{-half,half,.12f}}},1);
  auto ground_proto=prototype(s,std::move(ground),{asphalt,pavement},id+"/ground-source",catalog.id);city->material_sources.push_back(s.instances[ground_proto].id);
  for(int by=0;by<c.blocks_y;++by)for(int bx=0;bx<c.blocks_x;++bx){
    const auto key=id+"/block/"+std::to_string(bx)+"_"+std::to_string(by);if(progress)progress("生成街区 "+std::to_string(city->cells.size()+1)+" / "+std::to_string(c.blocks_x*c.blocks_y));
    Cell cell;cell.id=key;const float x=(bx-(c.blocks_x-1)*.5f)*pitch,y=(by-(c.blocks_y-1)*.5f)*pitch;cell.bounds.add({x-pitch*.5f,y-pitch*.5f,0});cell.bounds.add({x+pitch*.5f,y+pitch*.5f,0});
    cell.ground=instance(s,ground_proto,key+"/ground",id,key,origin*ir::Transform::translate({x,y,0}),true);city->materials.push_back({cell.ground,s.instances[ground_proto].id});
    ir::Mesh merged;merged.smooth=false;for(const auto &a:assets)merged.material_slots.push_back(a.name);
    for(int ly=0;ly<c.lots;++ly)for(int lx=0;lx<c.lots;++lx){
      const uint64_t seed=mix(c.seed)^mix(uint64_t(bx)*0x100000001ull+uint64_t(by)*0x10000ull+uint64_t(lx)*256+ly);if(random(seed)>c.density)continue;
      const size_t ai=mix(seed+1)%assets.size();const auto &a=assets[ai];const float width=a.bounds.maximum.x-a.bounds.minimum.x,depth=a.bounds.maximum.y-a.bounds.minimum.y;
      const float fit=std::min(1.f,(lot-3)/std::max(width,depth));if(fit<=.2f)throw std::runtime_error("地块过小，请增大街区边长或减少每边地块数");
      const float scale=fit*float(.92+.08*random(seed+2));const int turn=int(mix(seed+3)%4);const float angle=turn*1.57079632679f,cs=std::cos(angle)*scale,sn=std::sin(angle)*scale;
      ir::Transform placement;placement.value={cs,-sn,0,x-half+(lx+.5f)*lot,sn,cs,0,y-half+(ly+.5f)*lot,0,0,scale,.12f};
      Building b;b.id=key+"/lot/"+std::to_string(lx)+"_"+std::to_string(ly);b.asset=ai;b.cell=city->cells.size();b.transform=placement;b.landmark=random(seed+4)>.97&&a.bounds.maximum.z>25;
      for(int k=0;k<8;++k)b.bounds.add(placement.point({k&1?a.bounds.maximum.x:a.bounds.minimum.x,k&2?a.bounds.maximum.y:a.bounds.minimum.y,k&4?a.bounds.maximum.z:a.bounds.minimum.z}));
      cell.bounds.add(b.bounds.maximum);for(int level=0;level<2;++level){const auto entry=instance(s,prototypes[ai][level],b.id+"/LOD"+std::to_string(level+1),id,key,origin*placement,level==0);cell.instances[level].push_back(entry);city->materials.push_back({entry,s.instances[prototypes[ai][0]].id});}
      append_mesh(merged,boxes[ai],placement,uint32_t(ai));city->buildings.push_back(std::move(b));++cell.buildings;
    }
    if(!merged.triangles.empty()){auto proto=prototype(s,std::move(merged),proxy_materials,key+"/LOD3-source",catalog.id);cell.instances[2].push_back(instance(s,proto,key+"/LOD3",id,key,origin));}
    city->cells.push_back(std::move(cell));
  }
  if(city->buildings.empty())throw std::runtime_error("当前种子和密度未生成建筑，请提高密度或更换种子");
  // Boundary-aligned height-field patches preserve dense vertical steps. A flat, thin cage
  // is raised by a float displacement atlas; unlike a uniform grid it needs no micro-grid
  // between buildings. Evaluated geometry is retained for accurate preview and picking.
  for(int ry=0;ry<c.blocks_y;ry+=4)for(int rx=0;rx<c.blocks_x;rx+=4){
    Region region;for(int y=ry;y<std::min(ry+4,c.blocks_y);++y)for(int x=rx;x<std::min(rx+4,c.blocks_x);++x)region.cells.push_back(size_t(y*c.blocks_x+x));
    ir::Mesh mesh;mesh.smooth=false;mesh.material_slots={"CityHeight"};std::vector<float> heights{0};const auto key=id+"/region/"+std::to_string(rx/4)+"_"+std::to_string(ry/4);
    std::vector<std::pair<size_t,size_t>> ranges;
    for(const auto &b:city->buildings)if(std::find(region.cells.begin(),region.cells.end(),b.cell)!=region.cells.end()){
      if(b.landmark){auto proto=prototype(s,boxes[b.asset],{proxy_materials[b.asset]},b.id+"/landmark-source",catalog.id);region.landmarks.push_back(instance(s,proto,b.id+"/landmark",id,key,origin*b.transform));continue;}
      const auto first=mesh.positions.size();box(mesh,b.bounds.minimum,b.bounds.maximum);ranges.emplace_back(first,mesh.positions.size());heights.push_back(b.bounds.maximum.z-b.bounds.minimum.z-.01f);
    }
    if(!mesh.triangles.empty()){
      mesh.displacement_rest=mesh.positions;
      for(size_t k=0;k<ranges.size();++k){const auto [begin,end]=ranges[k];float z=mesh.positions[begin].z;for(size_t v=begin;v<end;++v)if(mesh.positions[v].z>z+.001f)mesh.displacement_rest[v].z=z+.01f;
        for(size_t t=(begin/4)*2;t<(end/4)*2;++t)for(int corner=0;corner<3;++corner){const bool top=mesh.positions[mesh.triangles[t].vertices[corner]].z>z+.001f;mesh.triangles[t].uv[corner]={float((top?k+1:0)+.5)/float(heights.size()),.5f};}
      }
      ir::Texture texture;texture.id=key+"/height";texture.file=height_image(heights);texture.colorspace=ir::ColorSpace::linear;const auto ti=int(s.textures.size());s.textures.push_back(texture);
      ir::Material material;material.id=key+"/material";material.base_color={.34f,.35f,.36f};material.roughness=.8f;material.displacement_texture=ti;material.displacement_min=0;material.displacement_max=1;material.displacement_strength=1;material.displacement_vertical=true;material.displacement_bump=false;
      auto mat=add_material(s,material);auto proto=prototype(s,std::move(mesh),{mat},key+"/source",catalog.id);region.proxy=instance(s,proto,key+"/LOD4",id,key,origin);
    }
    city->regions.push_back(std::move(region));
  }
  result.loaded.report={{"content_roots",nlohmann::json::array()},{"warnings",city->warnings},{"city_buildings",city->buildings.size()}};s.validate();result.city=std::move(city);return result;
}
}
