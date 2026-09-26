void emission_test(const fs::path &path,const Json &duf) {
  auto input=duf;
  input["material_library"][0]["extra"]=Json::parse(R"([{"channels":[
    {"channel":{"id":"Emission Color","value":[1,0.5,0.25],"image_file":"height.png"}},
    {"channel":{"id":"Luminance","value":200,"image_file":"height.png"}},
    {"channel":{"id":"Luminance Units","value":5}},
    {"channel":{"id":"Luminous Efficacy","value":15}},
    {"channel":{"id":"Emission Temperature","value":2900}},
    {"channel":{"id":"Thin Walled","value":true}},
    {"channel":{"id":"Two Sided Light","value":true}}
  ]}])");
  input["scene"]["materials"][0]["extra"]=Json::parse(R"([{"channels":[{"channel":{"id":"Luminance","current_value":550}}]}])");
  write(path,input);const auto loaded=dfv::daz::load(path);auto scene=loaded.scene;
  const auto &a=scene.materials[0],&b=scene.materials[1];
  require(a.emission_luminance==550&&b.emission_luminance==200&&a.emission_temperature==2900&&a.emission_two_sided,"自发光实例覆盖、色温或双面状态丢失");
  require(a.emission_color.x==1&&a.emission_color.y>.21f&&a.emission_color.y<.22f,"自发光颜色未转换到线性空间");
  require(a.emission_color_texture>=0&&a.emission_luminance_texture>=0&&scene.textures[a.emission_color_texture].colorspace==dfv::ir::ColorSpace::srgb&&scene.textures[a.emission_luminance_texture].colorspace==dfv::ir::ColorSpace::linear,"发光色与亮度纹理色彩空间错误");
  auto strengths=dfv::ir::emission_strengths(scene);require(std::abs(strengths[0]-.275f)<1e-6f&&std::abs(strengths[1]-.1f)<1e-6f,"瓦数没有按源表面世界面积换算");
  scene.instances[0].transform.value[0]*=2;scene.instances[0].transform.value[10]*=2;
  strengths=dfv::ir::emission_strengths(scene);require(std::abs(strengths[0]-.275f/4)<1e-6f,"灯具面积扩大四倍后功率未守恒");
  auto copy=scene.instances[0];copy.prototype=0;scene.instances.push_back(copy);
  require(dfv::ir::emission_strengths(scene)==strengths,"散布副本稀释了原型发光功率");
  auto m=b;m.emission_luminance=30000;m.emission_units=0;
  require(dfv::ir::emission_strength(m)==1,"cd/m² 基准错误");
  m.emission_units=1;require(dfv::ir::emission_strength(m)==1000,"kcd/m² 换算错误");
  m.emission_units=2;require(std::abs(dfv::ir::emission_strength(m)-10.76391f)<1e-4f,"平方英尺换算错误");
  m.emission_units=3;require(dfv::ir::emission_strength(m)==10000,"平方厘米换算错误");
  m.emission_units=4;require(dfv::ir::emission_strength(m,2)==.5f,"流明未按面积换算");
  m.emission_color={0,0,0};require(dfv::ir::emission_strength(m)==0,"黑色发光通道不应启用照明");
  input["material_library"][0]["extra"][0]["channels"][5]["channel"]["value"]=false;
  write(path,input);require(!dfv::daz::load(path).scene.materials[0].emission_two_sided,"非薄壁误启用双面发光");
  m.emission_luminance=std::numeric_limits<float>::infinity();bool rejected=false;
  try{dfv::ir::validate(m,scene.textures.size());}catch(const std::exception &){rejected=true;}require(rejected,"未拒绝无穷大自发光");
}
