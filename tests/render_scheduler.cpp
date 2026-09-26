#include "integrator/render_scheduler.h"
#include "session/session.h"
#include "session/tile.h"
#include <iostream>
#include <stdexcept>

// 使用真实调度器和上报计时，不依赖 GPU 负载，也不通过 sleep 制造结果。
static void check(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
static void run(bool adaptive,bool headless,double interval,double sample_seconds,bool expect_update) {
  ccl::SessionParams params;params.background=false;params.headless=headless;
  params.use_resolution_divider=false;params.dfv_update_interval=interval;
  ccl::TileManager tiles;
  ccl::RenderScheduler scheduler(tiles,params);
  ccl::AdaptiveSampling settings;settings.use=adaptive;settings.min_samples=32;settings.adaptive_step=4;settings.threshold=.01f;
  scheduler.set_adaptive_sampling(settings);scheduler.set_sample_params(4096,false,0,0);
  scheduler.set_limit_samples_per_update(1);
  ccl::BufferParams buffers;buffers.width=buffers.full_width=594;buffers.height=buffers.full_height=835;
  scheduler.reset(buffers);
  auto first=scheduler.get_render_work();scheduler.report_work_begin(first);
  scheduler.report_path_trace_time(first,sample_seconds,false);
  scheduler.report_display_update_time(first,.0005);
  auto next=scheduler.get_render_work();
  check(next.path_trace.num_samples==1,"第二批必须保留一个样本");
  check(next.display.update==expect_update,"自适应模式显示调度与预期不一致");
  check(scheduler.is_adaptive_sampling_used()==adaptive,"不能通过关闭自适应规避问题");
}
int main() {try {
  run(true,false,.125,.5,true);
  run(true,false,.125,.01,false);
  run(false,false,.125,.5,true);
  run(true,true,.125,.5,false);
  run(true,false,0,.5,false);
  std::cout<<"render_scheduler: PASS\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
