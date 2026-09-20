#include "bench/fixtures.h"
#include <OpenImageIO/imageio.h>
#include <numbers>
#include <stdexcept>

namespace dfv {
static void texture_file(const std::filesystem::path &path,int index) {
  if(std::filesystem::exists(path)) return;
  constexpr int size=2048;std::vector<unsigned char> data(size*size*4);
  for(int y=0;y<size;++y) for(int x=0;x<size;++x) {
    const int pixel=(y*size+x)*4;const bool checker=((x/128+y/128)%2)==0;
    data[pixel]=static_cast<unsigned char>(checker?180+(index*7)%65:35+(index*13)%80);
    data[pixel+1]=static_cast<unsigned char>(checker?90+(index*11)%120:25+(index*3)%90);
    data[pixel+2]=static_cast<unsigned char>(checker?80+(index*17)%130:30+(index*5)%70);data[pixel+3]=255;
  }
  auto output=OIIO::ImageOutput::create(path.string());
  if(!output || !output->open(path.string(),OIIO::ImageSpec(size,size,4,OIIO::TypeDesc::UINT8)) ||
     !output->write_image(OIIO::TypeDesc::UINT8,data.data()) || !output->close()) throw std::runtime_error("生成基准贴图失败");
}
ir::Scene build_fixture(bool medium,const std::filesystem::path &assets) {
  ir::Scene scene;std::filesystem::create_directories(assets);
  const int unique=medium?64:4,repeats=medium?2:1,segments=medium?128:48,rings=medium?64:24;
  for(int i=0;i<(medium?32:4);++i) {
    ir::Material m;m.id="bench-material-"+std::to_string(i);
    m.base_color={.2f+.6f*((i*7)%13)/13,.15f+.6f*((i*3)%11)/11,.3f};m.roughness=.15f+.7f*(i%5)/4;m.metallic=i%4==0?.8f:0;
    if(medium) {const auto path=assets/("checker-"+std::to_string(i)+".png");texture_file(path,i);m.color_texture=int(scene.textures.size());scene.textures.push_back({m.id,path});m.base_color={1,1,1};}
    scene.materials.push_back(m);
  }
  for(int i=0;i<unique;++i) {
    ir::Mesh mesh;mesh.id="bench-mesh-"+std::to_string(i);mesh.material_slots={"surface"};
    const float radius=medium?.45f:.9f;
    for(int y=0;y<=rings;++y) for(int x=0;x<=segments;++x) {
      const float theta=2*std::numbers::pi_v<float>*float(x)/segments,phi=std::numbers::pi_v<float>*float(y)/rings;
      const float r=radius*(1+.045f*std::sin(theta*(3+i%4))*std::sin(phi*5));
      mesh.positions.push_back({r*std::sin(phi)*std::cos(theta),r*std::sin(phi)*std::sin(theta),radius*std::cos(phi)});
    }
    for(int y=0;y<rings;++y) for(int x=0;x<segments;++x) {
      const auto a=uint32_t(y*(segments+1)+x),b=a+1,c=a+segments+1,d=c+1;
      for(const auto vertices:{std::array<uint32_t,3>{a,c,b},std::array<uint32_t,3>{b,c,d}}) {
        ir::Triangle tri;tri.vertices=vertices;
        for(int k=0;k<3;++k) tri.uv[k]={float(vertices[k]%(segments+1))/segments,float(vertices[k]/(segments+1))/rings};
        mesh.triangles.push_back(tri);
      }
    }
    const auto index=uint32_t(scene.meshes.size());scene.meshes.push_back(std::move(mesh));
    for(int j=0;j<repeats;++j) {
      const int k=i+j*unique;ir::Instance object;object.id="bench-instance-"+std::to_string(k);object.mesh=index;
      object.materials={uint32_t(i%scene.materials.size())};
      object.transform=ir::Transform::translate({medium?(k%16-7.5f)*1.1f:(i-1.5f)*2.1f,medium?(k/16-3.5f)*1.1f:0,medium?.55f:1});scene.instances.push_back(object);
    }
  }
  ir::Material floor;floor.id="bench-floor";floor.base_color={.2f+.6f*((40*7)%13)/13,.15f+.6f*((40*3)%11)/11,.3f};floor.roughness=.15f;floor.metallic=.8f;
  const auto material=uint32_t(scene.materials.size());scene.materials.push_back(floor);
  ir::Mesh mesh;mesh.id="bench-floor";mesh.smooth=false;mesh.material_slots={"floor"};mesh.positions={{-30,-30,0},{30,-30,0},{30,30,0},{-30,30,0}};
  ir::Triangle a,b;a.vertices={0,1,2};b.vertices={0,2,3};mesh.triangles={a,b};
  ir::Instance object;object.id="bench-floor";object.mesh=uint32_t(scene.meshes.size());object.materials={material};scene.meshes.push_back(mesh);scene.instances.push_back(object);
  for(int i=0;i<4;++i) {
    ir::AreaLight light;light.id="bench-light-"+std::to_string(i);light.width=light.height=4;light.power={500,450,400};
    light.transform=ir::Transform::translate({(i%2?1.0f:-1.0f)*6,(i/2?1.0f:-1.0f)*5,8});scene.lights.push_back(light);
  }
  return scene;
}
}
