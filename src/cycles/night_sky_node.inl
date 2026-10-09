NODE_DEFINE(DfvNightSkyNode)
{
  NodeType *type=NodeType::add("dfv_night_sky",create,NodeType::SHADER);
  SOCKET_IN_POINT(vector,"Vector",zero_float3(),SocketType::LINK_TEXTURE_GENERATED);
  SOCKET_INT(quality,"Quality",1);
  SOCKET_FLOAT(contrast,"Contrast",1.0f);
  SOCKET_FLOAT(density,"Density",1.0f);
  SOCKET_OUT_COLOR(galaxy,"Galaxy");
  SOCKET_OUT_COLOR(stars,"Stars");
  return type;
}
DfvNightSkyNode::DfvNightSkyNode():ShaderNode(get_node_type()){}
void DfvNightSkyNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,NODE_DFV_NIGHT_SKY,SVMNodeDfvNightSky{
      .quality=quality,.contrast=contrast,.density=density,
      .vector=compiler.input_link("Vector"),.galaxy_offset=compiler.output("Galaxy"),.stars_offset=compiler.output("Stars")});
}
void DfvNightSkyNode::compile(OSLCompiler &compiler){compiler.add(this,"dfv_night_sky");}
