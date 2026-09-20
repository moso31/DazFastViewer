#pragma once
#include "daz/morphs.h"
#include "cycles/adapter.h"
#include "viewport/window.h"
#include "bench/telemetry.h"
#include <memory>
#include <thread>

namespace dfv::editor {
struct Document {uint64_t generation=0;daz::LoadedScene loaded;daz::MorphCatalog catalog;};
struct Snapshot {uint64_t generation=0,revision=0;std::vector<runtime::Properties> values;};
struct RenderStatus {
  uint64_t generation=0,applied_revision=0,presented_revision=0,frames=0;
  uint64_t requested_epoch=0,presented_epoch=0;
  AdapterStats adapter;
  runtime::EvaluationStats evaluation;
  double max_displacement=0;
  int samples=0;
  std::string error;
};
class Renderer {
  std::filesystem::path output_;
  Telemetry telemetry_;
  std::unique_ptr<Window> window_;
  std::mutex mutex_;
  std::shared_ptr<const Document> document_;
  Snapshot snapshot_;
  RenderStatus status_;
  std::jthread thread_;
  void run(std::stop_token stop);
public:
  Renderer(HWND host,int width,int height,const std::filesystem::path &output);
  ~Renderer();
  void set_document(std::shared_ptr<const Document> document,const Snapshot &snapshot);
  void edit(const Snapshot &snapshot);
  RenderStatus status();
  void orbit(float x,float y);
};
}
