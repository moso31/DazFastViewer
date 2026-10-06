// All density and collision work stays on the GPU; the graph has at most 16 sweeps.
static void cloud_shader(ccl::Shader &shader,const ir::Material &material,ccl::Scene &scene){
  using namespace ccl;const auto &c=*material.cloud;auto graph=make_unique<ShaderGraph>();
  auto value=[&](float v){auto *n=graph->create_node<ValueNode>();n->set_value(v);return n->output("Value");};
  auto vec=[&](ir::Vec3 v){auto *n=graph->create_node<ColorNode>();n->set_value(vector(v));return n->output("Color");};
  auto math=[&](NodeMathType op,ShaderOutput *a,ShaderOutput *b){auto *n=graph->create_node<MathNode>();n->set_math_type(op);graph->connect(a,n->input("Value1"));graph->connect(b,n->input("Value2"));return n->output("Value");};
  auto vm=[&](NodeVectorMathType op,ShaderOutput *a,ShaderOutput *b,bool scalar=false){auto *n=graph->create_node<VectorMathNode>();n->set_math_type(op);graph->connect(a,n->input("Vector1"));graph->connect(b,n->input(op==NODE_VECTOR_MATH_SCALE?"Scale":"Vector2"));return n->output(scalar?"Value":"Vector");};
  auto clamp=[&](ShaderOutput *v){return math(NODE_MATH_MINIMUM,value(1),math(NODE_MATH_MAXIMUM,value(0),v));};
  auto smooth=[&](ShaderOutput *v){auto *u=clamp(v);return math(NODE_MATH_MULTIPLY,math(NODE_MATH_MULTIPLY,u,u),math(NODE_MATH_SUBTRACT,value(3),math(NODE_MATH_MULTIPLY,value(2),u)));};
  auto *coordinates=graph->create_node<TextureCoordinateNode>();auto *position=coordinates->output("Object");
  auto *noise=graph->create_node<NoiseTextureNode>();noise->set_dimensions(3);noise->set_scale(1/c.scale);noise->set_detail(c.detail);noise->set_roughness(.55f);
  graph->connect(vm(NODE_VECTOR_MATH_ADD,position,vec(c.offset)),noise->input("Vector"));
  auto *normalized=vm(NODE_VECTOR_MATH_DIVIDE,vm(NODE_VECTOR_MATH_ABSOLUTE,position,vec({})),vec(c.half_extent));
  auto *split=graph->create_node<SeparateXYZNode>();graph->connect(normalized,split->input("Vector"));
  // Raise the noise threshold towards the top/bottom: isolated billows, rather
  // than a uniformly filled fog box. Zero coverage is exactly empty.
  auto *vertical=math(NODE_MATH_MULTIPLY,split->output("Z"),split->output("Z"));
  auto *threshold=math(NODE_MATH_ADD,value(.75f-.4f*c.coverage),math(NODE_MATH_MULTIPLY,value(.18f),vertical));
  auto *density=math(NODE_MATH_MULTIPLY,value(c.coverage>0?c.density*8:0),clamp(math(NODE_MATH_SUBTRACT,noise->output("Fac"),threshold)));
  auto *edge=math(NODE_MATH_MAXIMUM,split->output("X"),math(NODE_MATH_MAXIMUM,split->output("Y"),split->output("Z")));
  density=math(NODE_MATH_MULTIPLY,density,smooth(math(NODE_MATH_MULTIPLY,value(6.666667f),math(NODE_MATH_SUBTRACT,value(1),edge))));
  for(const auto &o:c.colliders){
    auto *q=vm(NODE_VECTOR_MATH_SUBTRACT,position,vec(o.center));
    auto *lo=value(-1e30f),*hi=value(1e30f);ShaderOutput *side=nullptr;
    auto inside=[&](ShaderOutput *depth){return o.softness>0?smooth(math(NODE_MATH_DIVIDE,depth,value(o.softness))):math(NODE_MATH_GREATER_THAN,depth,value(0));};
    // Intersect [0,1] with the actual convex hull along the motion segment.
    // No enclosing sphere and no outward feather: only padding expands a face.
    for(const auto &p:o.planes){
      const float b=p.normal.x*o.tail.x+p.normal.y*o.tail.y+p.normal.z*o.tail.z;
      // Bake constant division/sign into the plane, saving one GPU operation
      // per face at every volume sample (up to 768 faces per cloud).
      if(std::abs(b)<1e-7f){auto *depth=math(NODE_MATH_SUBTRACT,value(p.distance+o.padding),vm(NODE_VECTOR_MATH_DOT_PRODUCT,q,vec(p.normal),true));side=side?math(NODE_MATH_MINIMUM,side,depth):depth;}
      else {
        auto *t=math(NODE_MATH_SUBTRACT,vm(NODE_VECTOR_MATH_DOT_PRODUCT,q,vec({p.normal.x/b,p.normal.y/b,p.normal.z/b}),true),value((p.distance+o.padding)/b));
        if(b>0)lo=math(NODE_MATH_MAXIMUM,lo,t);else hi=math(NODE_MATH_MINIMUM,hi,t);
      }
    }
    auto *strength=side?inside(side):value(1);
    if(o.tail_length>0){
      auto *depth=math(NODE_MATH_MINIMUM,math(NODE_MATH_MINIMUM,hi,math(NODE_MATH_SUBTRACT,value(1),lo)),math(NODE_MATH_MULTIPLY,math(NODE_MATH_SUBTRACT,hi,lo),value(.5f)));
      strength=math(NODE_MATH_MULTIPLY,strength,inside(math(NODE_MATH_MULTIPLY,depth,value(o.tail_length))));
    }
    auto *recovery=math(NODE_MATH_EXPONENT,math(NODE_MATH_MULTIPLY,clamp(lo),value(-o.decay)),value(0));
    auto *keep=math(NODE_MATH_SUBTRACT,value(1),math(NODE_MATH_MULTIPLY,strength,recovery));
    density=math(NODE_MATH_MULTIPLY,density,keep);
  }
  auto *scatter=graph->create_node<ScatterVolumeNode>();scatter->set_color(make_float3(.98f,.985f,1));scatter->set_anisotropy(.35f);graph->connect(density,scatter->input("Density"));graph->connect(scatter->output("Volume"),graph->output()->input("Volume"));
  // Cycles uses 0.1 * mean world bounds as its procedural-volume base step.
  // Budget the longest local axis. A diagonal takes at most sqrt(3) times this.
  const auto h=c.half_extent;const float step=2*std::max({h.x,h.y,h.z})/c.steps;
  shader.set_volume_step_rate(step/(.2f*(h.x+h.y+h.z)/3));
  shader.name=ustring(material.id);shader.set_graph(std::move(graph));shader.tag_update(&scene);
}
