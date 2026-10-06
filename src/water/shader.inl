// Native water never enters the DAZ layered-material conversion.
static void water_shader(ccl::Shader &shader,const ir::Material &material,ccl::Scene &scene,bool detail){
  using namespace ccl;const auto &w=*material.water;auto graph=make_unique<ShaderGraph>();
  auto value=[&](float v){auto *n=graph->create_node<ValueNode>();n->set_value(v);return n->output("Value");};
  auto math=[&](NodeMathType op,ShaderOutput *a,ShaderOutput *b){auto *n=graph->create_node<MathNode>();n->set_math_type(op);graph->connect(a,n->input("Value1"));graph->connect(b,n->input("Value2"));return n->output("Value");};
  auto mix=[&](ShaderOutput *a,ShaderOutput *b,ShaderOutput *f){auto *n=graph->create_node<MixClosureNode>();graph->connect(a,n->input("Closure1"));graph->connect(b,n->input("Closure2"));graph->connect(f,n->input("Fac"));return n->output("Closure");};
  auto *geometry=graph->create_node<GeometryNode>();auto *uv=graph->create_node<UVMapNode>();uv->set_attribute(ustring("UVMap"));
  auto *offset=graph->create_node<VectorMathNode>();offset->set_math_type(NODE_VECTOR_MATH_ADD);offset->set_vector2(make_float3(std::fmod(w.time*.17f,1000.f)+float(w.seed%997),std::fmod(w.time*.11f,1000.f),0));graph->connect(uv->output("UV"),offset->input("Vector1"));
  auto *noise=graph->create_node<NoiseTextureNode>();noise->set_dimensions(3);noise->set_scale(1.7f);noise->set_detail(3);noise->set_roughness(.55f);graph->connect(offset->output("Vector"),noise->input("Vector"));
  ShaderOutput *normal=geometry->output("Normal");
  if(detail&&w.ripples>0){auto *bump=graph->create_node<BumpNode>();bump->set_strength(std::min(1.f,w.ripples));bump->set_distance(.065f);graph->connect(noise->output("Fac"),bump->input("Height"));normal=bump->output("Normal");}
  auto *fresnel=graph->create_node<FresnelNode>();fresnel->set_IOR(1.333f);graph->connect(normal,fresnel->input("Normal"));
  auto *reflection=graph->create_node<GlossyBsdfNode>();reflection->set_color(one_float3());reflection->set_roughness(material.roughness);graph->connect(normal,reflection->input("Normal"));
  // Uniform authored optical depth: angle-adjusted homogeneous attenuation.
  auto *cosine=graph->create_node<VectorMathNode>();cosine->set_math_type(NODE_VECTOR_MATH_DOT_PRODUCT);graph->connect(geometry->output("Incoming"),cosine->input("Vector1"));graph->connect(normal,cosine->input("Vector2"));
  auto *cos2=math(NODE_MATH_MULTIPLY,cosine->output("Value"),cosine->output("Value"));
  auto *refracted2=math(NODE_MATH_SUBTRACT,value(1),math(NODE_MATH_MULTIPLY,math(NODE_MATH_SUBTRACT,value(1),cos2),value(1/(1.333f*1.333f))));
  auto *path=math(NODE_MATH_DIVIDE,value(-w.depth/std::max(.01f,w.clarity)),math(NODE_MATH_SQRT,refracted2,value(0)));
  auto *transmittance=math(NODE_MATH_EXPONENT,path,value(0));
  auto *refraction=graph->create_node<RefractionBsdfNode>();refraction->set_color(one_float3());refraction->set_IOR(1.333f);refraction->set_roughness(material.roughness);graph->connect(normal,refraction->input("Normal"));
  auto *body=graph->create_node<DiffuseBsdfNode>();body->set_color(vector(material.base_color));body->set_roughness(.2f);graph->connect(normal,body->input("Normal"));
  auto *surface=mix(mix(body->output("BSDF"),refraction->output("BSDF"),transmittance),reflection->output("BSDF"),fresnel->output("Fac"));
  auto *coverage=graph->create_node<AttributeNode>();coverage->set_attribute(ustring("dfv_water_foam"));
  // Foam tiling is independent of the fine-wave bump and contact-band width.
  auto *foam_noise=noise;
  if(w.foam_uv_scale!=1){foam_noise=graph->create_node<NoiseTextureNode>();foam_noise->set_dimensions(3);foam_noise->set_scale(1.7f*w.foam_uv_scale);foam_noise->set_detail(3);foam_noise->set_roughness(.55f);graph->connect(offset->output("Vector"),foam_noise->input("Vector"));}
  auto *broken=math(NODE_MATH_MULTIPLY,math(NODE_MATH_GREATER_THAN,foam_noise->output("Fac"),value(.46f)),math(NODE_MATH_MULTIPLY,coverage->output("Fac"),value(w.foam)));
  auto *foam=graph->create_node<DiffuseBsdfNode>();foam->set_color(make_float3(.72f,.77f,.76f));foam->set_roughness(.6f);
  surface=mix(surface,foam->output("BSDF"),broken);graph->connect(surface,graph->output()->input("Surface"));shader.name=ustring(material.id);shader.set_graph(std::move(graph));shader.tag_update(&scene);
}
