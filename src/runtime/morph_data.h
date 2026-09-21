#pragma once
#include "render_ir/scene.h"
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <span>

namespace dfv::runtime {
struct SparseOffset {uint32_t vertex=0;ir::Vec3 delta;};
using OffsetBuffer=std::vector<SparseOffset>;
enum class PayloadState {unloaded,loading,ready,failed};
void trim_morph_cache();
class MorphPayload:public std::enable_shared_from_this<MorphPayload> {
  mutable std::mutex mutex_;std::condition_variable changed_;
  PayloadState state_=PayloadState::unloaded;
  std::shared_ptr<const OffsetBuffer> data_;
  std::string error_;std::function<OffsetBuffer()> loader_;
  void run();
public:
  const std::string id;
  const size_t count,vertices;
  MorphPayload(std::string identity,size_t size,size_t vertex_count,std::function<OffsetBuffer()> loader)
    :loader_(std::move(loader)),id(std::move(identity)),count(size),vertices(vertex_count) {}
  void request(bool retry=false);
  std::shared_ptr<const OffsetBuffer> acquire() const;
  std::shared_ptr<const OffsetBuffer> ensure();
  PayloadState state() const;
  std::string error() const;
  size_t evict();
};
struct OffsetView {
  std::shared_ptr<const OffsetBuffer> owner;
  std::span<const SparseOffset> values;
  explicit OffsetView(const OffsetBuffer &v):values(v) {}
  explicit OffsetView(std::shared_ptr<const OffsetBuffer> v):owner(std::move(v)),values(*owner) {}
  auto begin() const{return values.begin();}auto end() const{return values.end();}
  size_t size() const{return values.size();}
};
}
