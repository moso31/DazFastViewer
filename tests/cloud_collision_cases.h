static void convex_collision_cases(){
  auto collider=[](const cloud::Hull &h){ir::CloudCollider c;c.center=h.center;c.planes=h.planes;c.softness=0;return c;};
  const std::vector<ir::Vec3> tetra={{0,0,0},{10,0,0},{0,10,0},{0,0,10}};
  auto c=collider(cloud::convex_hull(tetra));check(c.planes.size()==4,"tetrahedron lost its actual convex faces");
  check(cloud::transmission(c,{1,1,1})==0&&cloud::transmission(c,{4,4,4})==1,"zero padding did not follow the mesh convex hull");
  c.softness=100;check(cloud::transmission(c,{4,4,4})==1,"feather expanded outside the convex hull");
  c.softness=0;c.padding=.5f;check(cloud::transmission(c,{3.5f,3.5f,3.5f})==0&&cloud::transmission(c,{4,4,4})==1,"padding did not use world-distance plane offsets");
  std::vector<ir::Vec3> box;for(float x:{-50.f,50.f})for(float y:{-1.f,1.f})for(float z:{-1.f,1.f})box.push_back({x,y,z});
  ir::Transform rotation;const float r=std::sqrt(.5f);rotation.value={r,-r,0,0,r,r,0,0,0,0,1,0};
  auto h=cloud::transformed_hull(cloud::convex_hull(box),rotation);c=collider(h);
  check(cloud::transmission(c,{20,20,0})==0&&cloud::transmission(c,{20,-20,0})==1,"rotated thin object still clears its enclosing sphere/AABB");
  c.tail={0,0,100};c.tail_length=100;c.decay=2;
  const auto wake=cloud::transmission(c,{20,20,50});check(wake>0&&wake<1&&cloud::transmission(c,{20,-20,50})==1,"swept hull lost its cross-section or recovery");
  check(cloud::transmission(c,{20,20,102})==1,"wake extends beyond the bounded segment");
  c.softness=.5f;c.tail={0,0,.0001f};c.tail_length=.0001f;
  check(cloud::transmission(c,{20,20,0})==0,"a short wake restored cloud inside the object");
  check(cloud::transmission(c,{20,20,1.01f})==1,"short-wake feather expanded beyond the object");
  std::vector<ir::Vec3> unit;for(float x:{-1.f,1.f})for(float y:{-1.f,1.f})for(float z:{-1.f,1.f})unit.push_back({x,y,z});
  auto scale=rotation;scale.value[0]*=2;scale.value[4]*=2;scale.value[1]*=3;scale.value[5]*=3;scale.value[10]=4;scale.value[3]=10000;scale.value[7]=-5000;
  c=collider(cloud::transformed_hull(cloud::convex_hull(unit),scale));const ir::Vec3 at{10000+2.5f*r,-5000+2.5f*r,0};
  c.padding=.49f;check(cloud::transmission(c,at)==1,"nonuniform scale enlarged the padding");c.padding=.51f;check(cloud::transmission(c,at)==0,"nonuniform scale shrank the padding");
  const std::vector<ir::Vec3> plane={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};c=collider(cloud::convex_hull(plane));check(c.planes.size()==6,"planar mesh lacks finite edge constraints");
  check(cloud::transmission(c,{0,0,.01f})==1,"flat mesh acquired a hidden minimum thickness");c.padding=.02f;check(cloud::transmission(c,{0,0,.01f})==0&&cloud::transmission(c,{2,0,.01f})==1,"planar hull padding ignores its edges");
  std::vector<ir::Vec3> sphere;for(int i=0;i<10000;++i){const float z=1-2*(i+.5f)/10000,rad=std::sqrt(1-z*z),a=i*2.39996323f;sphere.push_back({rad*std::cos(a),rad*std::sin(a),z});}
  const auto bounded=cloud::convex_hull(sphere);check(bounded.vertices.size()<=cloud::hull_vertex_budget&&bounded.planes.size()<=ir::max_cloud_planes&&!bounded.planes.empty(),"dense mesh exceeded collision shader budget");
}
