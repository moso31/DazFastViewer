// 独立可行性探针：检查真实引擎的刚体、Z-up 地面及移动锚点布料。
// 数值断言与求解耗时不代表 DAZ 资产效果或渲染窗口交互已验收。
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace JPH;
using Clock = std::chrono::steady_clock;
constexpr float dt = 1.0f / 60.0f;

struct BroadLayers final : BroadPhaseLayerInterface {
  uint GetNumBroadPhaseLayers() const override { return 2; }
  BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override { return BroadPhaseLayer(uint8(l)); }
};
struct PairFilter final : ObjectLayerPairFilter {
  bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a != 0 || b != 0; }
};
struct BroadFilter final : ObjectVsBroadPhaseLayerFilter {
  bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a != 0 || b == BroadPhaseLayer(1); }
};
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
double ms(Clock::time_point start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }

struct World {
  BroadLayers layers;
  PairFilter pair;
  BroadFilter broad;
  TempAllocatorImpl temp{32 * 1024 * 1024};
  // 一个调用线程和一个工作线程；不默认占满所有 CPU。
  JobSystemThreadPool jobs{cMaxPhysicsJobs, cMaxPhysicsBarriers, 1};
  PhysicsSystem physics;
  World() {
    physics.Init(1024, 0, 1024, 1024, layers, broad, pair);
    physics.SetGravity(Vec3(0, 0, -9.81f));
    auto settings = physics.GetPhysicsSettings();
    settings.mPenetrationSlop = .001f;
    settings.mSpeculativeContactDistance = .005f;
    physics.SetPhysicsSettings(settings);
  }
  BodyID add(const Shape *shape, Vec3 position, EMotionType motion) {
    return physics.GetBodyInterface().CreateAndAddBody(
        BodyCreationSettings(shape, RVec3(position), Quat::sIdentity(), motion,
                             motion == EMotionType::Static ? 0 : 1),
        motion == EMotionType::Static ? EActivation::DontActivate : EActivation::Activate);
  }
  void floor() { add(new PlaneShape(Plane(Vec3::sAxisZ(), 0)), Vec3::sZero(), EMotionType::Static); }
  void step() { require(physics.Update(dt, 2, &temp, &jobs) == EPhysicsUpdateError::None, "physics update error"); }
};

void rigid_probe(bool ground) {
  World world;
  if (ground) world.floor();
  auto &bodies = world.physics.GetBodyInterface();
  const BodyID box = world.add(new BoxShape(Vec3::sReplicate(.1f)), Vec3(0, 0, 2), EMotionType::Dynamic);
  for (int i = 0; i < (ground ? 300 : 60); ++i) world.step();
  const float height = float(bodies.GetPosition(box).GetZ());
  require(std::isfinite(height) && (ground ? std::abs(height - .1f) < .005f : height < -2.0f), "rigid ground check failed");
  std::cout << "{\"case\":\"rigid_" << (ground ? "ground" : "no_ground") << "\",\"height_m\":" << height << ",\"pass\":true}\n";
}

void cloth_probe(uint n, bool dihedral) {
  World world;
  world.floor();
  const Vec3 sphere_center(0, .7f, .65f);
  constexpr float sphere_radius = .35f;
  world.add(new SphereShape(sphere_radius), sphere_center, EMotionType::Static);
  Ref<SoftBodySharedSettings> mesh = new SoftBodySharedSettings;
  const auto begin = Clock::now();
  for (uint y = 0; y < n; ++y) for (uint x = 0; x < n; ++x) {
    SoftBodySharedSettings::Vertex v;
    const float u = float(x) / (n - 1), t = float(y) / (n - 1);
    v.mPosition = Float3(u - .5f, 1.5f * t, 1.5f);
    v.mInvMass = y == 0 ? 0.0f : float(n * n) / .3f;
    mesh->mVertices.push_back(v);
    SoftBodySharedSettings::Skinned skin(y * n + x, y == 0 ? 0.0f : 2.0f, FLT_MAX, 0.0f);
    skin.mWeights[0] = SoftBodySharedSettings::SkinWeight(0, 1.0f);
    mesh->mSkinnedConstraints.push_back(skin);
  }
  for (uint y = 0; y + 1 < n; ++y) for (uint x = 0; x + 1 < n; ++x) {
    const uint a = y * n + x, b = a + 1, c = a + n, d = c + 1;
    mesh->AddFace(SoftBodySharedSettings::Face(a, b, d));
    mesh->AddFace(SoftBodySharedSettings::Face(a, d, c));
  }
  SoftBodySharedSettings::VertexAttributes attributes(1e-7f, 1e-7f, 1e-4f,
      SoftBodySharedSettings::ELRAType::GeodesicDistance, 1.05f);
  mesh->CreateConstraints(&attributes, 1, dihedral ? SoftBodySharedSettings::EBendType::Dihedral : SoftBodySharedSettings::EBendType::Distance);
  mesh->mInvBindMatrices.push_back(SoftBodySharedSettings::InvBind(0, Mat44::sIdentity()));
  mesh->CalculateSkinnedConstraintNormals();
  mesh->Optimize();
  SoftBodyCreationSettings settings(mesh, RVec3::sZero(), Quat::sIdentity(), 1);
  settings.mUpdatePosition = false;
  settings.mAllowSleeping = false;
  settings.mNumIterations = 8;
  settings.mVertexRadius = .003f;
  settings.mLinearDamping = 1.0f;
  Body *cloth = world.physics.GetBodyInterface().CreateSoftBody(settings);
  require(cloth != nullptr, "soft body allocation failed");
  world.physics.GetBodyInterface().AddBody(cloth->GetID(), EActivation::Activate);
  auto *motion = static_cast<SoftBodyMotionProperties *>(cloth->GetMotionProperties());
  Mat44 pose = Mat44::sIdentity();
  motion->SkinVertices(cloth->GetCenterOfMassTransform(), &pose, 1, true, world.temp);
  const double prepare_ms = ms(begin);
  std::vector<double> timings;
  float max_pin_error = 0, min_ground = 1e9f, min_sphere = 1e9f, final_tip_z = 0;
  bool finite = true;
  for (int frame = 0; frame < 360; ++frame) {
    const float shift = .15f * std::sin(frame * dt * 2.0f);
    const auto start = Clock::now();
    pose = Mat44::sTranslation(Vec3(shift, 0, 0));
    motion->SkinVertices(cloth->GetCenterOfMassTransform(), &pose, 1, false, world.temp);
    world.step();
    if (frame >= 30) timings.push_back(ms(start));
    const auto &vertices = motion->GetVertices();
    for (uint i = 0; i < vertices.size(); ++i) {
      const Vec3 p = vertices[i].mPosition;
      finite &= std::isfinite(p.GetX()) && std::isfinite(p.GetY()) && std::isfinite(p.GetZ());
      min_ground = std::min(min_ground, p.GetZ());
      min_sphere = std::min(min_sphere, (p - sphere_center).Length() - sphere_radius);
      if (i < n) {
        const Vec3 target = Vec3::sLoadFloat3Unsafe(mesh->mVertices[i].mPosition) + Vec3(shift, 0, 0);
        max_pin_error = std::max(max_pin_error, (p - target).Length());
      }
    }
    final_tip_z = vertices[n * n - n / 2 - 1].mPosition.GetZ();
  }
  std::sort(timings.begin(), timings.end());
  double sum = 0; for (double t : timings) sum += t;
  const bool pass = finite && max_pin_error < .005f && min_ground > -.025f && min_sphere > -.025f && final_tip_z < 1.35f;
  std::cout << "{\"case\":\"skinned_cloth\",\"vertices\":" << n * n
            << ",\"triangles\":" << mesh->mFaces.size() << ",\"prepare_ms\":" << prepare_ms
            << ",\"mean_ms\":" << sum / timings.size() << ",\"p95_ms\":" << timings[size_t(.95 * (timings.size() - 1))]
            << ",\"max_ms\":" << timings.back() << ",\"pin_error_m\":" << max_pin_error
            << ",\"min_ground_m\":" << min_ground << ",\"min_sphere_gap_m\":" << min_sphere
            << ",\"final_tip_z_m\":" << final_tip_z << ",\"pass\":" << (pass ? "true" : "false") << "}\n";
  require(pass, "cloth numerical checks failed");
}

int main(int argc, char **argv) {
  RegisterDefaultAllocator();
  Factory::sInstance = new Factory;
  RegisterTypes();
  int result = 0;
  try {
    const bool dihedral = argc > 1 && std::string_view(argv[1]) == "--dihedral";
    std::cout << "{\"engine\":\"Jolt v5.6.0\",\"bend\":\"" << (dihedral ? "dihedral" : "distance")
              << "\",\"dt\":0.016666667,\"collision_steps\":2,\"soft_iterations\":8,\"job_workers\":1,\"gpu\":false}\n";
    rigid_probe(true);
    rigid_probe(false);
    for (uint size : {33u, 65u, 129u}) cloth_probe(size, dihedral);
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; result = 1; }
  UnregisterTypes();
  delete Factory::sInstance;
  Factory::sInstance = nullptr;
  return result;
}
