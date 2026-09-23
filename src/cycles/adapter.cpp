#include "cycles/adapter.h"
#include "render_ir/options.h"
#include "render_ir/sun_sky.h"
#include "scene/scene.h"
#include "scene/camera.h"
#include "scene/mesh.h"
#include "scene/hair.h"
#include "scene/object.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/background.h"
#include "scene/light.h"
#include "scene/attribute.h"
#include "util/colorspace.h"
#include "util/transform.h"
#include <OpenImageIO/imageio.h>
#include <map>
#include <stdexcept>
#include <cmath>
#include <cfloat>

namespace dfv {
static ccl::float3 vector(ir::Vec3 v) {return ccl::make_float3(v.x,v.y,v.z);}
static ccl::Transform transform(const ir::Transform &t) {
  ccl::Transform out;const auto &v=t.value;
  out.x=ccl::make_float4(v[0],v[1],v[2],v[3]);out.y=ccl::make_float4(v[4],v[5],v[6],v[7]);out.z=ccl::make_float4(v[8],v[9],v[10],v[11]);return out;
}
void CyclesAdapter::material(ccl::Shader &shader,const ir::Material &m,float texel_distance) {
  using namespace ccl;
  auto graph=make_unique<ShaderGraph>();
  auto *uv=graph->create_node<UVMapNode>();uv->set_attribute(ustring("UVMap"));
  auto *scale=graph->create_node<VectorMathNode>();scale->set_math_type(NODE_VECTOR_MATH_MULTIPLY);scale->set_vector2(make_float3(m.uv_scale.x,m.uv_scale.y,1));
  graph->connect(uv->output("UV"),scale->input("Vector1"));
  auto *offset=graph->create_node<VectorMathNode>();offset->set_math_type(NODE_VECTOR_MATH_ADD);offset->set_vector2(make_float3(m.uv_offset.x,m.uv_offset.y,0));
  graph->connect(scale->output("Vector"),offset->input("Vector1"));
  auto math=[&](NodeMathType operation,ShaderOutput *a,float b) {
    auto *n=graph->create_node<MathNode>();n->set_math_type(operation);graph->connect(a,n->input("Value1"));n->set_value2(b);return n->output("Value");
  };
  std::map<int,ShaderOutput *> images;
  auto texture=[&](int index)->ShaderOutput * {
    if(images.contains(index)) return images.at(index);
    const auto &t=textures_.at(index);
    auto image_node=[&](const std::filesystem::path &file,ShaderOutput *coords,bool raw) {
      auto *node=graph->create_node<ImageTextureNode>();graph->connect(coords,node->input("Vector"));
      const auto utf8=file.u8string();node->set_filename(ustring(std::string(utf8.begin(),utf8.end())));
      node->set_colorspace(raw||t.colorspace==ir::ColorSpace::linear?u_colorspace_data:u_colorspace_scene_linear_srgb);return node;
    };
    ShaderOutput *result=nullptr;
    if(t.layers.empty()) result=image_node(t.file,offset->output("Vector"),false)->output("Color");
    else {
      for(const auto &layer:t.layers) {
        ShaderOutput *coords=offset->output("Vector");
        if(layer.rotation!=0||layer.offset.x!=0||layer.offset.y!=0||layer.scale.x!=1||layer.scale.y!=1||layer.mirror_x||layer.mirror_y) {
          auto *shift=graph->create_node<VectorMathNode>();shift->set_math_type(NODE_VECTOR_MATH_SUBTRACT);shift->set_vector2(make_float3(.5f+layer.offset.x,.5f+layer.offset.y,0));graph->connect(coords,shift->input("Vector1"));
          auto *rotate=graph->create_node<VectorRotateNode>();rotate->set_axis(make_float3(0,0,1));rotate->set_angle(-layer.rotation*.01745329252f);graph->connect(shift->output("Vector"),rotate->input("Vector"));
          auto *resize=graph->create_node<VectorMathNode>();resize->set_math_type(NODE_VECTOR_MATH_MULTIPLY);resize->set_vector2(make_float3((layer.mirror_x?-1.f:1.f)/std::max(1e-6f,layer.scale.x),(layer.mirror_y?-1.f:1.f)/std::max(1e-6f,layer.scale.y),1));graph->connect(rotate->output("Vector"),resize->input("Vector1"));
          auto *center=graph->create_node<VectorMathNode>();center->set_math_type(NODE_VECTOR_MATH_ADD);center->set_vector2(make_float3(.5f,.5f,0));graph->connect(resize->output("Vector"),center->input("Vector1"));coords=center->output("Vector");
        }
        ShaderOutput *pixels=nullptr,*alpha=nullptr;
        if(!layer.file.empty()) {auto *n=image_node(layer.file,coords,true);pixels=n->output("Color");alpha=math(NODE_MATH_MULTIPLY,n->output("Alpha"),layer.opacity);}
        else {auto *n=graph->create_node<ColorNode>();n->set_value(vector(layer.color));pixels=n->output("Color");}
        if(layer.invert) {auto *n=graph->create_node<VectorMathNode>();n->set_math_type(NODE_VECTOR_MATH_SUBTRACT);n->set_vector1(one_float3());graph->connect(pixels,n->input("Vector2"));pixels=n->output("Vector");}
        auto *blend=graph->create_node<MixNode>();
        blend->set_mix_type(!result||layer.operation=="alpha_blend"?NODE_MIX_BLEND:layer.operation=="multiply"?NODE_MIX_MUL:layer.operation=="add"?NODE_MIX_ADD:NODE_MIX_SUB);
        blend->set_use_clamp(true);blend->set_color1(zero_float3());blend->set_fac(layer.opacity);
        if(result) graph->connect(result,blend->input("Color1"));graph->connect(pixels,blend->input("Color2"));if(alpha) graph->connect(alpha,blend->input("Fac"));result=blend->output("Color");
      }
      if(t.gamma>0) {auto *gamma=graph->create_node<GammaNode>();gamma->set_gamma(t.gamma);graph->connect(result,gamma->input("Color"));result=gamma->output("Color");}
      else if(t.colorspace==ir::ColorSpace::srgb) {
        auto *separate=graph->create_node<SeparateXYZNode>();graph->connect(result,separate->input("Vector"));auto *combine=graph->create_node<CombineXYZNode>();
        for(const auto *axis:{"X","Y","Z"}) {
          auto *value=separate->output(axis);auto *low=math(NODE_MATH_MULTIPLY,value,1.f/12.92f);
          auto *high=math(NODE_MATH_POWER,math(NODE_MATH_MULTIPLY,math(NODE_MATH_ADD,value,.055f),1.f/1.055f),2.4f);
          auto *select=graph->create_node<MixFloatNode>();graph->connect(math(NODE_MATH_LESS_THAN,value,.04045f),select->input("Factor"));graph->connect(high,select->input("A"));graph->connect(low,select->input("B"));graph->connect(select->output("Result"),combine->input(axis));
        }
        result=combine->output("Vector");
      }
    }
    images[index]=result;return result;
  };
  auto scalar=[&](int index,float value,bool invert=false)->ShaderOutput * {
    auto *node=graph->create_node<MathNode>();node->set_math_type(NODE_MATH_MULTIPLY);node->set_value1(value);node->set_value2(1);
    if(index>=0) graph->connect(texture(index),node->input("Value2"));
    if(!invert) return node->output("Value");
    auto *subtract=graph->create_node<MathNode>();subtract->set_math_type(NODE_MATH_SUBTRACT);subtract->set_value1(1);
    graph->connect(node->output("Value"),subtract->input("Value2"));return subtract->output("Value");
  };
  auto color=[&](int index,ir::Vec3 value)->ShaderOutput * {
    auto *node=graph->create_node<VectorMathNode>();node->set_math_type(NODE_VECTOR_MATH_MULTIPLY);node->set_vector1(vector(value));node->set_vector2(one_float3());
    if(index>=0) graph->connect(texture(index),node->input("Vector2"));return node->output("Vector");
  };
  auto mix=[&](ShaderOutput *a,ShaderOutput *b,ShaderOutput *factor) {
    auto *node=graph->create_node<MixClosureNode>();graph->connect(a,node->input("Closure1"));graph->connect(b,node->input("Closure2"));graph->connect(factor,node->input("Fac"));return node->output("Closure");
  };
  ShaderOutput *surface_normal=nullptr;
  const bool displace=m.displacement_texture>=0&&m.displacement_strength!=0&&(m.displacement_min!=0||m.displacement_max!=0);
  shader.set_displacement_method(displace?DISPLACE_BOTH:DISPLACE_BUMP);
  if(displace) {
    auto *range=graph->create_node<MathNode>();range->set_math_type(NODE_MATH_MULTIPLY);range->set_value2(m.displacement_max-m.displacement_min);graph->connect(texture(m.displacement_texture),range->input("Value1"));
    auto *height=graph->create_node<MathNode>();height->set_math_type(NODE_MATH_ADD);height->set_value2(m.displacement_min);graph->connect(range->output("Value"),height->input("Value1"));
    auto *node=graph->create_node<DisplacementNode>();node->set_space(NODE_NORMAL_MAP_OBJECT);node->set_midlevel(0);node->set_scale(m.displacement_strength);
    graph->connect(height->output("Value"),node->input("Height"));graph->connect(node->output("Displacement"),graph->output()->input("Displacement"));
  }
  if(m.normal_texture>=0) {
    auto *normal=graph->create_node<NormalMapNode>();normal->set_strength(m.normal_strength);normal->set_attribute(ustring("UVMap"));
    graph->connect(texture(m.normal_texture),normal->input("Color"));surface_normal=normal->output("Normal");
  }
  if(m.bump_texture>=0&&m.bump_strength>0&&m.bump_distance>0) {
    auto *bump=graph->create_node<BumpNode>();bump->set_strength(std::min(1.f,m.bump_strength));
    // Cycles Strength 是法线插值权重（上限 1），DAZ 的 >1 强度应放大高度。
    const float distance=m.bump_from_texel_density&&texel_distance>0?texel_distance:m.bump_distance;
    bump->set_distance(distance*std::max(1.f,m.bump_strength));
    bump->set_invert(m.bump_invert);
    graph->connect(texture(m.bump_texture),bump->input("Height"));if(surface_normal) graph->connect(surface_normal,bump->input("Normal"));surface_normal=bump->output("Normal");
  }
  ShaderOutput *surface=nullptr;
  auto *base=color(m.color_texture,m.base_color);
  if(m.hair) {
    auto *hair=graph->create_node<PrincipledHairBsdfNode>();hair->set_roughness(m.roughness);hair->set_radial_roughness(m.hair_radial_roughness);
    auto *info=graph->create_node<HairInfoNode>();auto *blend=graph->create_node<MixNode>();blend->set_mix_type(NODE_MIX_BLEND);blend->set_color2(vector(m.hair_tip_color));
    graph->connect(base,blend->input("Color1"));graph->connect(info->output("Intercept"),blend->input("Fac"));
    hair->set_parametrization(m.hair_melanin>0?NODE_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION:NODE_PRINCIPLED_HAIR_REFLECTANCE);
    hair->set_melanin(m.hair_melanin);hair->set_melanin_redness(m.hair_redness);
    graph->connect(blend->output("Color"),hair->input(m.hair_melanin>0?"Tint":"Color"));surface=hair->output("BSDF");
  } else {
    auto *sss=scalar(m.translucency_texture,m.subsurface);
    ShaderOutput *sss_color=base;
    if(m.separate_subsurface_color&&m.subsurface>0) {
      const ir::Vec3 tint={m.translucency_color.x*m.subsurface_color.x,m.translucency_color.y*m.subsurface_color.y,m.translucency_color.z*m.subsurface_color.z};
      sss_color=color(m.translucency_color_texture>=0?m.translucency_color_texture:m.color_texture,tint);
    }
    auto *coat=scalar(m.coat_texture,m.coat),*coat_rough=scalar(m.coat_roughness_texture,m.coat_roughness);
    auto *metallic=scalar(m.metallic_texture,m.metallic),*transmission=scalar(m.transmission_texture,m.transmission);
    auto principled=[&](ShaderOutput *rough,ShaderOutput *specular,ir::Vec3 tint,int tint_texture=-1) {
      auto *node=graph->create_node<PrincipledBsdfNode>();node->set_ior(m.ior);node->set_thin_wall(m.thin_walled);
      node->set_subsurface_method(CLOSURE_BSSRDF_RANDOM_WALK_ID);node->set_subsurface_radius(vector(m.subsurface_radius));node->set_subsurface_scale(1);node->set_subsurface_anisotropy(m.subsurface_anisotropy);
      node->set_anisotropic(m.anisotropy);node->set_anisotropic_rotation(m.anisotropy_rotation);node->set_specular_tint(vector(tint));
      if(tint_texture>=0) graph->connect(color(tint_texture,tint),node->input("Specular Tint"));
      node->set_coat_ior(m.coat_ior);node->set_coat_tint(vector(m.coat_color));
      graph->connect(base,node->input("Base Color"));graph->connect(rough,node->input("Roughness"));graph->connect(specular,node->input("Specular IOR Level"));
      graph->connect(metallic,node->input("Metallic"));graph->connect(transmission,node->input("Transmission Weight"));
      node->set_coat_weight(0);graph->connect(coat_rough,node->input("Coat Roughness"));
      if(surface_normal) {graph->connect(surface_normal,node->input("Normal"));graph->connect(surface_normal,node->input("Coat Normal"));}
      if(!m.separate_subsurface_color||m.subsurface<=0) {graph->connect(sss,node->input("Subsurface Weight"));return node->output("BSDF");}
      // Principled 共用漫反射与 SSS 颜色，因此拆为两份同光学参数的闭包再按透光权重混合。
      auto *scatter=graph->create_node<PrincipledBsdfNode>();scatter->set_ior(m.ior);scatter->set_subsurface_weight(1);
      scatter->set_subsurface_method(CLOSURE_BSSRDF_RANDOM_WALK_ID);scatter->set_subsurface_radius(vector(m.subsurface_radius));scatter->set_subsurface_scale(1);scatter->set_subsurface_anisotropy(m.subsurface_anisotropy);
      scatter->set_anisotropic(m.anisotropy);scatter->set_anisotropic_rotation(m.anisotropy_rotation);
      graph->connect(color(tint_texture,tint),scatter->input("Specular Tint"));graph->connect(sss_color,scatter->input("Base Color"));graph->connect(rough,scatter->input("Roughness"));graph->connect(specular,scatter->input("Specular IOR Level"));
      graph->connect(metallic,scatter->input("Metallic"));graph->connect(transmission,scatter->input("Transmission Weight"));if(surface_normal) graph->connect(surface_normal,scatter->input("Normal"));
      return mix(node->output("BSDF"),scatter->output("BSDF"),sss);
    };
    auto *rough=scalar(m.roughness_texture,m.roughness,m.roughness_from_glossiness);
    surface=principled(rough,m.weighted_glossy?scalar(-1,0):scalar(m.specular_texture,m.specular),m.specular_color,m.specular_color_texture);
    if(m.weighted_glossy&&m.specular>0) {
      auto *gloss=graph->create_node<GlossyBsdfNode>();gloss->set_color(vector(m.specular_color));gloss->set_anisotropy(m.anisotropy);gloss->set_rotation(m.anisotropy_rotation);
      graph->connect(color(m.specular_color_texture,m.specular_color),gloss->input("Color"));
      graph->connect(rough,gloss->input("Roughness"));if(surface_normal) graph->connect(surface_normal,gloss->input("Normal"));
      surface=mix(surface,gloss->output("BSDF"),scalar(m.specular_texture,m.specular));
    }
    if(m.dual_weight>0) {
      auto *a=principled(scalar(-1,m.dual_roughness1),scalar(-1,m.dual_specular),{1,1,1});
      auto *b=principled(scalar(-1,m.dual_roughness2),scalar(-1,m.dual_specular),{1,1,1});
      surface=mix(surface,mix(b,a,scalar(-1,m.dual_ratio)),scalar(m.dual_texture,m.dual_weight));
    }
    if(m.coat>0) {
      // Top Coat 反射色不能直接作为 Principled Coat Tint 的吸收色。
      auto *gloss=graph->create_node<GlossyBsdfNode>();gloss->set_color(vector(m.coat_color));graph->connect(coat_rough,gloss->input("Roughness"));
      graph->connect(color(m.coat_color_texture,m.coat_color),gloss->input("Color"));
      if(surface_normal) graph->connect(surface_normal,gloss->input("Normal"));
      ShaderOutput *fresnel=nullptr;
      if(m.coat_mode==1) fresnel=scalar(-1,1);
      else if(m.coat_mode==2) {auto *f=graph->create_node<FresnelNode>();f->set_IOR(m.coat_ior);if(surface_normal) graph->connect(surface_normal,f->input("Normal"));fresnel=f->output("Fac");}
      else {
        auto *layer=graph->create_node<LayerWeightNode>();layer->set_blend(.5f);if(surface_normal) graph->connect(surface_normal,layer->input("Normal"));
        auto *power=graph->create_node<MathNode>();power->set_math_type(NODE_MATH_POWER);power->set_value2(m.coat_exponent);graph->connect(layer->output("Facing"),power->input("Value1"));
        auto *scale=graph->create_node<MathNode>();scale->set_math_type(NODE_MATH_MULTIPLY);scale->set_value2(m.coat_grazing-m.coat_normal);graph->connect(power->output("Value"),scale->input("Value1"));
        auto *bias=graph->create_node<MathNode>();bias->set_math_type(NODE_MATH_ADD);bias->set_value2(m.coat_normal);graph->connect(scale->output("Value"),bias->input("Value1"));fresnel=bias->output("Value");
      }
      auto *weight=graph->create_node<MathNode>();weight->set_math_type(NODE_MATH_MULTIPLY);graph->connect(coat,weight->input("Value1"));graph->connect(fresnel,weight->input("Value2"));surface=mix(surface,gloss->output("BSDF"),weight->output("Value"));
    }
    if(m.translucency>0) {
      auto *translucent=graph->create_node<TranslucentBsdfNode>();graph->connect(color(m.translucency_color_texture,m.translucency_color),translucent->input("Color"));
      if(surface_normal) graph->connect(surface_normal,translucent->input("Normal"));
      surface=mix(surface,translucent->output("BSDF"),scalar(m.translucency_texture,m.translucency));
    }
  }
  if(m.opacity<1||m.opacity_texture>=0) {
    auto *transparent=graph->create_node<TransparentBsdfNode>();transparent->set_color(one_float3());
    surface=mix(transparent->output("BSDF"),surface,scalar(m.opacity_texture,m.opacity));
  }
  graph->connect(surface,graph->output()->input("Surface"));
  shader.name=ustring(m.id);shader.set_graph(std::move(graph));shader.tag_update(&scene_);
}
void CyclesAdapter::environment(const ir::RenderOptions &options) {
  using namespace ccl;options_=options;
  auto graph=make_unique<ShaderGraph>();auto *bg=graph->create_node<BackgroundNode>();
  const auto &n=options.environment;const int mode=int(ir::number(n,"Environment Mode",0));
  bg->set_color(vector(n.id.empty()?environment_:ir::color(n,"Environment Tint")));
  bg->set_strength(n.id.empty()?1:mode==3?0:float(ir::number(n,"Environment Intensity",1)*ir::number(n,"Environment Map",1)));
  if(mode==2) {
    auto *sky=graph->create_node<SkyTextureNode>();sky->set_sky_type(NODE_SKY_MULTIPLE_SCATTERING);
    const auto direction=ir::solar_direction(n);
    sky->set_sun_elevation(std::asin(std::clamp(direction.z,-1.f,1.f)));
    sky->set_sun_rotation(std::atan2(direction.x,direction.y));
    const float scale=std::clamp(float(ir::number(n,"SS Sun Disk Scale",4)),.01f,100.f);
    sky->set_sun_size(.009512f*scale);
    // Cycles 按太阳盘立体角归一化能量；不能再次除以尺寸平方。
    const float energy=ir::number(n,"SS Physically Scaled Sun",1)?1:scale*scale;
    sky->set_sun_intensity(std::max(0.f,float(ir::number(n,"SS Sun Disk Intensity",1)))*energy);
    sky->set_aerosol_density(std::max(0.f,float(ir::number(n,"SS Haze",0)))+1);
    sky->set_altitude(0);
    auto *tint=graph->create_node<VectorMathNode>();tint->set_math_type(NODE_VECTOR_MATH_MULTIPLY);tint->set_vector2(vector(ir::color(n,"Environment Tint")));
    graph->connect(sky->output("Color"),tint->input("Vector1"));graph->connect(tint->output("Vector"),bg->input("Color"));
    // 现有 HDRI/EV13 预览尺度下的辐射单位归一化；不声称 Iray 绝对测光等价。
    double units=ir::number(n,"SS RGB Unit Conversion",1);if(units==0) units=1.0/80000;
    bg->set_strength(float(std::max(0.0,ir::number(n,"Environment Intensity",1)*ir::number(n,"SS Multiplier",.10132)*units)));
  }
  if(!options.environment_file.empty()&&(mode==0||mode==1)) {
    auto *tex=graph->create_node<EnvironmentTextureNode>();const auto path=options.environment_file.u8string();tex->set_filename(ustring(std::string(path.begin(),path.end())));tex->set_colorspace(u_colorspace_data);
    auto *coord=graph->create_node<TextureCoordinateNode>();ShaderOutput *direction=coord->output("Generated");
    // 逆向采样穹顶旋转，并转换 DAZ Y 向上坐标。
    const std::array<std::pair<float3,double>,4> rotations={{{make_float3(0,0,1),ir::number(n,"Dome Orientation Y",0)},{make_float3(1,0,0),ir::number(n,"Dome Orientation X",0)},{make_float3(0,1,0),-ir::number(n,"Dome Orientation Z",0)},{make_float3(0,0,1),-ir::number(n,"Dome Rotation",0)}}};
    for(const auto &[axis,degrees]:rotations) if(degrees!=0) {auto *r=graph->create_node<VectorRotateNode>();r->set_axis(axis);r->set_angle(float(degrees*.017453292519943));graph->connect(direction,r->input("Vector"));direction=r->output("Vector");}
    graph->connect(direction,tex->input("Vector"));
    auto *tint=graph->create_node<VectorMathNode>();tint->set_math_type(NODE_VECTOR_MATH_MULTIPLY);tint->set_vector2(vector(ir::color(n,"Environment Tint")));graph->connect(tex->output("Color"),tint->input("Vector1"));graph->connect(tint->output("Vector"),bg->input("Color"));
  }
  ShaderOutput *surface=bg->output("Background");
  if(!n.id.empty()&&!ir::number(n,"Draw Dome",0)) {
    auto *back=graph->create_node<BackgroundNode>();back->set_color(make_float3(options.backdrop[0],options.backdrop[1],options.backdrop[2]));back->set_strength(1);
    auto *path=graph->create_node<LightPathNode>();auto *mix=graph->create_node<MixClosureNode>();graph->connect(surface,mix->input("Closure1"));graph->connect(back->output("Background"),mix->input("Closure2"));graph->connect(path->output("Is Camera Ray"),mix->input("Fac"));surface=mix->output("Closure");
  }
  graph->connect(surface,graph->output()->input("Surface"));scene_.default_background->set_graph(std::move(graph));scene_.default_background->tag_update(&scene_);
  // 背景着色器负责求值，背景灯负责按 HDRI 亮度分布采样；缺少后者会遗漏直接环境采样。
  if(!background_light_) {
    background_light_=scene_.create_light_node<BackgroundLight>();
    auto *object=scene_.create_node<Object>();object->set_geometry(background_light_);object->tag_update(&scene_);
    array<Node *> shaders;shaders.push_back_slow(scene_.default_background);background_light_->set_used_shaders(shaders);
  }
  background_light_->set_use_mis(mode!=3);background_light_->set_map_resolution(0);background_light_->tag_update(&scene_);
  for(size_t i=0;i<lights_.size();++i) {lights_[i]->set_strength(ir::scene_lights(options)?vector(light_power_[i]):zero_float3());lights_[i]->tag_update(&scene_);}
}
void CyclesAdapter::load(const ir::Scene &source) {
  using namespace ccl;
  if(loaded_) throw std::runtime_error("同一 CyclesAdapter 只允许一次完整加载，请使用 Delta 更新");
  source.validate();textures_=source.textures;stats_.textures=textures_.size();
  bump_distances_.assign(source.materials.size(),0);
  std::vector<double> world_area(source.materials.size()),uv_area(source.materials.size());
  for(const auto &instance:source.instances) if(instance.prototype<0) {
    const auto &mesh=source.meshes.at(instance.mesh);
    for(const auto &t:mesh.triangles) {
      const auto index=instance.materials.at(t.material_slot);const auto &m=source.materials.at(index);
      if(!m.bump_from_texel_density||m.bump_texture<0||m.bump_strength<=0) continue;
      const auto a=instance.transform.point(mesh.positions[t.vertices[0]]),b=instance.transform.point(mesh.positions[t.vertices[1]]),c=instance.transform.point(mesh.positions[t.vertices[2]]);
      const double x1=b.x-a.x,y1=b.y-a.y,z1=b.z-a.z,x2=c.x-a.x,y2=c.y-a.y,z2=c.z-a.z;
      const double x=y1*z2-z1*y2,y=z1*x2-x1*z2,z=x1*y2-y1*x2;
      const double u1=t.uv[1].x-t.uv[0].x,v1=t.uv[1].y-t.uv[0].y,u2=t.uv[2].x-t.uv[0].x,v2=t.uv[2].y-t.uv[0].y;
      world_area[index]+=std::sqrt(x*x+y*y+z*z);uv_area[index]+=std::abs((u1*v2-v1*u2)*m.uv_scale.x*m.uv_scale.y);
    }
  }
  std::map<std::filesystem::path,double> pixels;
  for(size_t i=0;i<source.materials.size();++i) if(world_area[i]>0&&uv_area[i]>0) {
    const auto &t=textures_.at(source.materials[i].bump_texture);
    if(!pixels.contains(t.file)) {const auto p=t.file.u8string();auto input=OIIO::ImageInput::open(std::string(p.begin(),p.end()));pixels[t.file]=input?double(input->spec().width)*input->spec().height:0;}
    if(pixels[t.file]>0) bump_distances_[i]=float(2*std::sqrt(world_area[i]/(uv_area[i]*pixels[t.file])));
  }
  meshes_.resize(source.meshes.size());hairs_.resize(source.meshes.size());for(const auto &m:source.meshes) vertex_counts_.push_back(m.positions.size());
  for(size_t i=0;i<source.materials.size();++i) {auto *shader=scene_.create_node<Shader>();material(*shader,source.materials[i],bump_distances_[i]);shaders_.push_back(shader);++stats_.materials;}
  std::map<std::pair<uint32_t,std::vector<uint32_t>>,Mesh *> meshes;
  std::map<std::pair<uint32_t,std::vector<uint32_t>>,Hair *> hairs;
  for(const auto &instance:source.instances) {
    const auto key=std::make_pair(instance.mesh,instance.materials);auto found=meshes.find(key);Mesh *mesh;
    const auto &data=source.meshes.at(instance.mesh);
    std::vector<Geometry *> geometries;
    if(!data.triangles.empty()) {
    if(found==meshes.end()) {
      const auto visible=std::count_if(data.triangles.begin(),data.triangles.end(),[&](const auto &t){return data.draws(t);});
      mesh=scene_.create_node<Mesh>();mesh->resize_mesh(int(data.positions.size()),int(visible));
      auto *positions=mesh->get_position_for_write();
      for(size_t i=0;i<data.positions.size();++i) positions[i]=vector(data.positions[i]);
      auto *uv=mesh->attributes.add(ATTR_STD_UV,ustring("UVMap"))->data_for_write<float2>();
      size_t i=0;for(const auto &t:data.triangles) {if(!data.draws(t)) continue;
        for(size_t k=0;k<3;++k) {mesh->get_triangles()[i*3+k]=int(t.vertices[k]);uv[i*3+k]=make_float2(t.uv[k].x,t.uv[k].y);}
        mesh->get_shader()[i]=t.material_slot;mesh->get_smooth()[i]=data.smooth;
        ++i;
      }
      array<Node *> shaders(instance.materials.size());for(size_t i=0;i<shaders.size();++i) shaders[i]=shaders_.at(instance.materials[i]);mesh->set_used_shaders(shaders);
      meshes.emplace(key,mesh);meshes_[instance.mesh].push_back(mesh);++stats_.meshes;stats_.unique_triangles+=visible;
    } else mesh=found->second;
    geometries.push_back(mesh);
    }
    if(!data.curves.empty()) {
      Hair *hair;
      if(auto cached=hairs.find(key);cached!=hairs.end()) hair=cached->second;
      else {
        hair=scene_.create_node<Hair>();size_t count=0;for(const auto &curve:data.curves) count+=curve.vertices.size();
        hair->resize_curves(int(data.curves.size()),int(count));hair->curve_shape=CURVE_THICK;
        auto *positions=hair->get_position_for_write();auto *radii=hair->get_radius_for_write();
        auto *uv=hair->attributes.add(ATTR_STD_UV,ustring("UVMap"))->data_for_write<float2>();
        auto *intercept=hair->attributes.add(ATTR_STD_CURVE_INTERCEPT)->data_for_write<float>();
        HairBinding binding;binding.hair=hair;binding.vertices.reserve(count);size_t first=0;
        for(size_t c=0;c<data.curves.size();++c) {
          const auto &curve=data.curves[c];const auto &material=source.materials.at(instance.materials.at(curve.material_slot));
          hair->get_curve_first_key()[c]=int(first);hair->get_curve_shader()[c]=int(curve.material_slot);uv[c]=make_float2(curve.uv.x,curve.uv.y);
          for(size_t k=0;k<curve.vertices.size();++k) {
            const float t=float(k)/float(curve.vertices.size()-1);const auto v=curve.vertices[k];positions[first]=vector(data.positions[v]);
            radii[first]=std::max(1e-8f,material.hair_root_radius*(1-t)+material.hair_tip_radius*t);intercept[first]=t;
            binding.vertices.push_back(v);++first;
          }
        }
        array<Node *> shaders(instance.materials.size());for(size_t i=0;i<shaders.size();++i) shaders[i]=shaders_.at(instance.materials[i]);hair->set_used_shaders(shaders);
        hairs.emplace(key,hair);hairs_[instance.mesh].push_back(std::move(binding));stats_.curves+=data.curves.size();
      }
      geometries.push_back(hair);
    }
    objects_.emplace_back();
    for(auto *geometry:geometries) {
      auto *object=scene_.create_node<Object>();object->name=ustring(instance.id);object->set_geometry(geometry);object->set_tfm(transform(instance.transform));
      object->set_visibility(instance.visible?PATH_RAY_VISIBILITY_ALL:0);objects_.back().push_back(object);
    }
    ++stats_.instances;if(!data.triangles.empty()) stats_.triangles+=mesh->num_triangles();
  }
  auto light_graph=make_unique<ShaderGraph>();auto *emission=light_graph->create_node<EmissionNode>();
  emission->set_color(one_float3());emission->set_strength(1);light_graph->connect(emission->output("Emission"),light_graph->output()->input("Surface"));
  scene_.default_light->set_graph(std::move(light_graph));scene_.default_light->tag_update(&scene_);
  for(const auto &data:source.lights) {
    Light *light=nullptr;
    if(data.kind==ir::LightKind::point) light=scene_.create_node<PointLight>();
    else if(data.kind==ir::LightKind::spot) {auto *spot=scene_.create_node<SpotLight>();spot->set_angle(data.angle);light=spot;}
    else if(data.kind==ir::LightKind::distant) light=scene_.create_node<SunLight>();
    else {auto *area=scene_.create_node<AreaLight>();area->set_sizeu(data.width);area->set_sizev(data.height);light=area;}
    light_power_.push_back(data.power);light->set_strength(vector(data.power));light->set_use_mis(true);lights_.push_back(light);
    auto *object=scene_.create_node<Object>();object->set_geometry(light);object->set_tfm(transform(data.transform));
    light_objects_.push_back(object);
  }
  environment_=source.environment;environment(source.options);
  loaded_=true;ir::Delta initial;initial.camera=source.camera;apply(initial);
}
void CyclesAdapter::apply(const ir::Delta &delta) {
  if(!loaded_) throw std::runtime_error("CyclesAdapter 尚未加载场景");
  // 先校验整个变更，避免索引错误导致只应用一部分。
  if(delta.camera) ir::validate(*delta.camera);
  for(const auto &edit:delta.lights) {
    if(edit.index>=lights_.size()) throw std::runtime_error("灯光索引越界");
    ir::Scene check;check.lights.push_back(edit.value);check.validate();
  }
  for(const auto &edit:delta.materials) {
    if(edit.index>=shaders_.size()) throw std::runtime_error("材质更新索引越界");
    ir::validate(edit.value,textures_.size());
  }
  for(const auto &edit:delta.meshes) {
    if(edit.index>=meshes_.size() || edit.positions.size()!=vertex_counts_[edit.index]) throw std::runtime_error("顶点 Delta 不能改变拓扑或越界");
    for(const auto &p:edit.positions) if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) throw std::runtime_error("顶点 Delta 含非有限值");
    for(const auto *mesh:meshes_[edit.index]) if(mesh->transform_applied) throw std::runtime_error("动态编辑要求未烘焙对象变换的动态 BVH 场景");
  }
  for(const auto &edit:delta.instances) {
    if(edit.index>=objects_.size()) throw std::runtime_error("实例 Delta 索引越界");
    for(float x:edit.transform.value) if(!std::isfinite(x)) throw std::runtime_error("实例变换含非有限值");
    for(auto *object:objects_[edit.index]) if(object->get_geometry()->transform_applied) throw std::runtime_error("对象变换已烘焙，不能直接动态修改");
  }
  for(const auto &edit:delta.visibility) if(edit.index>=objects_.size()) throw std::runtime_error("可见性实例索引越界");
  if(delta.options) environment(*delta.options);
  if(delta.camera) {
    const auto &c=*delta.camera;auto &camera=*scene_.camera;
    camera.set_camera_type(ccl::CAMERA_PERSPECTIVE);camera.set_full_width(c.width);camera.set_full_height(c.height);
    camera.set_fov(c.fov);camera.set_matrix(transform(c.transform));camera.compute_auto_viewplane();
    // Cycles 的 FLT_MAX 表示无限远裁剪；大地形聚焦后可远超默认 100 千米。
    camera.set_farclip(FLT_MAX);
    camera.need_device_update=true;camera.need_flags_update=true;++stats_.camera_updates;
  }
  for(const auto &edit:delta.materials) {
    auto *shader=shaders_.at(edit.index);
    // Cycles 的置换是原地写入位置；材质重算前恢复未置换的位置，避免反复编辑累加高度。
    for(const auto &variants:meshes_) for(auto *mesh:variants) {
      const auto &used=mesh->get_used_shaders();if(std::find(used.begin(),used.end(),shader)==used.end()) continue;
      if(const auto *base=mesh->attributes.find(ccl::ATTR_STD_POSITION_UNDISPLACED)) {
        const auto *positions=base->data<ccl::packed_float3>();auto *out=mesh->get_position_for_write();
        std::copy(positions,positions+mesh->num_verts(),out);
        for(auto attribute:{ccl::ATTR_STD_VERTEX_NORMAL,ccl::ATTR_STD_POSITION_UNDISPLACED,ccl::ATTR_STD_NORMAL_UNDISPLACED,ccl::ATTR_STD_UV_TANGENT_UNDISPLACED,ccl::ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED}) mesh->attributes.remove(attribute);
        mesh->tag_position_modified();mesh->tag_update(&scene_,false);
      }
    }
    material(*shader,edit.value,bump_distances_.at(edit.index));++stats_.material_updates;
  }
  for(const auto &edit:delta.lights) {
    light_power_[edit.index]=edit.value.power;lights_[edit.index]->set_strength(ir::scene_lights(options_)?vector(edit.value.power):ccl::zero_float3());lights_[edit.index]->tag_update(&scene_);
    auto *object=light_objects_[edit.index];object->set_tfm(transform(edit.value.transform));object->tag_update(&scene_);
  }
  for(const auto &edit:delta.meshes) {
    for(auto *mesh:meshes_[edit.index]) {
      auto *positions=mesh->get_position_for_write();
      for(size_t i=0;i<edit.positions.size();++i) positions[i]=vector(edit.positions[i]);
      for(auto attribute:{ccl::ATTR_STD_VERTEX_NORMAL,ccl::ATTR_STD_POSITION_UNDISPLACED,ccl::ATTR_STD_NORMAL_UNDISPLACED,ccl::ATTR_STD_UV_TANGENT_UNDISPLACED,ccl::ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED}) mesh->attributes.remove(attribute);
      mesh->tag_position_modified();
      mesh->compute_bounds();mesh->tag_update(&scene_,false);
    }
    for(const auto &binding:hairs_[edit.index]) {
      auto *positions=binding.hair->get_position_for_write();for(size_t k=0;k<binding.vertices.size();++k) positions[k]=vector(edit.positions[binding.vertices[k]]);
      binding.hair->tag_position_modified();binding.hair->compute_bounds();binding.hair->tag_update(&scene_,false);
    }
    ++stats_.geometry_updates;
  }
  for(const auto &edit:delta.instances) {
    for(auto *object:objects_[edit.index]) {object->set_tfm(transform(edit.transform));object->tag_update(&scene_);}++stats_.instance_updates;
  }
  for(const auto &edit:delta.visibility) for(auto *object:objects_[edit.index]) {object->set_visibility(edit.visible?ccl::PATH_RAY_VISIBILITY_ALL:0);object->tag_update(&scene_);}
}
}
