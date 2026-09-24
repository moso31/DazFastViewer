#include "viewport/display.h"
#include <stdexcept>

namespace dfv {
Display::Display(Window &window,Telemetry &telemetry,std::atomic<uint64_t> &epoch,std::atomic<int> &samples,bool readback)
 :window_(window),telemetry_(telemetry),epoch_(epoch),samples_(samples),allow_readback_(readback) {}
Display::~Display() {
  window_.render_context.activate();glFinish();
  for(auto &s:slots_) {if(s.fence) glDeleteSync(s.fence);if(s.texture) glDeleteTextures(1,&s.texture);}
  if(upload_) glDeleteSync(upload_);
  if(pbo_) glDeleteBuffers(1,&pbo_);
  if(retired_pbo_) glDeleteBuffers(1,&retired_pbo_);
  window_.render_context.deactivate();
}
void Display::allocate(int width,int height) {
  // 只扩容，不随几个像素或预览 / 完整质量切换反复分配。
  const size_t bytes=size_t((width+127)/128*128)*((height+127)/128*128)*sizeof(ccl::half4);
  if(bytes>pbo_bytes_) {
    retired_pbo_=pbo_;glGenBuffers(1,&pbo_);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo_);
    glBufferData(GL_PIXEL_UNPACK_BUFFER,bytes,nullptr,GL_DYNAMIC_DRAW);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    pbo_bytes_=bytes;interop_changed_=true;
  }
  // 此槽已等到呈现 fence；其他槽继续显示旧图，不触碰正在使用的纹理。
  auto &s=slots_[writing_];
  if(width>s.width||height>s.height) {
    s.width=std::max(s.width,(width+127)/128*128);s.height=std::max(s.height,(height+127)/128*128);
    if(!s.texture) glGenTextures(1,&s.texture);glBindTexture(GL_TEXTURE_2D,s.texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,s.width,s.height,0,GL_RGBA,GL_HALF_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  }
  glBindTexture(GL_TEXTURE_2D,0);
  if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("分配 OpenGL 输出资源失败");
}
bool Display::update_begin(const Params &p,int width,int height) {
  const auto begin=now();
  if(width<1 || height<1 || p.size.x!=width || p.size.y!=height) {
    error_="渲染尺寸与输出缓冲不一致";failed_=true;return false;
  }
  window_.render_context.activate();
  {
    std::lock_guard lock(slots_mutex_);
    for(int i=0;i<3;++i) {
      auto &s=slots_[i];
      if(s.state==State::retiring) {
        const GLenum result=glClientWaitSync(s.fence,0,0);
        if(result==GL_ALREADY_SIGNALED || result==GL_CONDITION_SATISFIED) {
          glDeleteSync(s.fence);s.fence=nullptr;s.state=State::idle;
        }
        else if(result==GL_WAIT_FAILED) {error_="OpenGL fence 查询失败";failed_=true;}
      }
      if(s.state==State::idle) {writing_=i;s.state=State::writing;break;}
    }
    // 场景同步暂时占用呈现线程时，覆盖尚未显示的最旧完成帧。
    // 正在显示的槽继续保留，不把短暂积压当成 interop 错误。
    if(writing_<0) {
      int oldest=-1;
      for(int i=0;i<3;++i) if(slots_[i].state==State::ready) {
        const auto result=glClientWaitSync(slots_[i].fence,0,0);
        if((result==GL_ALREADY_SIGNALED||result==GL_CONDITION_SATISFIED)&&
           (oldest<0||slots_[i].frame.id<slots_[oldest].frame.id)) oldest=i;
      }
      if(oldest>=0) {
        auto &s=slots_[oldest];glDeleteSync(s.fence);s.fence=nullptr;
        telemetry_.skipped++;writing_=oldest;s.state=State::writing;
      }
    }
  }
  if(writing_<0) {telemetry_.skipped++;window_.render_context.deactivate();return false;}
  if(upload_) {glWaitSync(upload_,0,GL_TIMEOUT_IGNORED);glDeleteSync(upload_);upload_=nullptr;}
  allocate(width,height);
  auto &s=slots_[writing_];const auto samples=samples_.load();
  s.frame={samples>0?telemetry_.produced.fetch_add(1)+1:0,epoch_.load(),samples,width,height,now()};
  telemetry_.event("display_begin",s.frame,(now()-begin)*1000);
  return true;
}
void Display::update_end() {
  const auto begin=now();
  // Cycles 在写入新 PBO 前已注销旧 CUDA 注册；现在才释放旧 GL 对象。
  if(retired_pbo_) {glDeleteBuffers(1,&retired_pbo_);retired_pbo_=0;}
  auto &s=slots_[writing_];
  // 收敛/取消后的空工作会更新显示缓冲，但不能覆盖最后有效帧。
  // 正常完成驱动回调，避免把主动丢弃误报为 interop 初始化失败。
  if(s.frame.samples<=0) {
    {std::lock_guard lock(slots_mutex_);s.state=State::idle;}
    telemetry_.skipped++;writing_=-1;window_.render_context.deactivate();return;
  }
  glBindTexture(GL_TEXTURE_2D,s.texture);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo_);
  glTexSubImage2D(GL_TEXTURE_2D,0,0,0,s.frame.width,s.frame.height,GL_RGBA,GL_HALF_FLOAT,nullptr);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glBindTexture(GL_TEXTURE_2D,0);
  upload_=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
  {
    std::lock_guard lock(slots_mutex_);
    s.fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);s.state=State::ready;
  }
  glFlush();telemetry_.event("display_upload",s.frame,(now()-begin)*1000);telemetry_.event("produced",s.frame);
  writing_=-1;window_.render_context.deactivate();
}
ccl::half4 *Display::map_texture_buffer() {
  if(!allow_readback_) {error_="GPU interop 不可用；显式 --allow-readback 才允许诊断回读";failed_=true;return nullptr;}
  telemetry_.readback_bytes+=size_t(slots_[writing_].frame.width)*slots_[writing_].frame.height*sizeof(ccl::half4);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo_);
  return static_cast<ccl::half4 *>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER,GL_WRITE_ONLY));
}
void Display::unmap_texture_buffer() {glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);}
ccl::GraphicsInteropDevice Display::graphics_interop_get_device() {
  ccl::GraphicsInteropDevice device;device.type=ccl::GraphicsInteropDevice::OPENGL;return device;
}
void Display::graphics_interop_update_buffer() {
  if(interop_changed_||graphics_interop_buffer_.is_empty()) {
    graphics_interop_buffer_.assign(ccl::GraphicsInteropDevice::OPENGL,pbo_,pbo_bytes_);interop_changed_=false;
  }
}
static GLuint shader(GLenum type,const char *source) {
  const GLuint id=glCreateShader(type);glShaderSource(id,1,&source,nullptr);glCompileShader(id);
  GLint ok;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
  if(!ok) {char log[2048];glGetShaderInfoLog(id,2048,nullptr,log);throw std::runtime_error(log);}
  return id;
}
void Display::make_program() {
  const auto v=shader(GL_VERTEX_SHADER,"#version 130\nout vec2 uv;void main(){gl_Position=gl_Vertex;uv=gl_MultiTexCoord0.xy;}");
  const auto f=shader(GL_FRAGMENT_SHADER,R"GLSL(#version 130
uniform sampler2D beauty;
uniform vec2 texture_scale;
uniform int enabled,per_component;
uniform vec4 tone; // exposure, burn, crush, saturation
uniform vec3 white;
uniform float gamma_value,vignette,aspect;
in vec2 uv;out vec4 color;
vec3 compress_color(vec3 x) {return x*(vec3(1)+tone.y*x)/(vec3(1)+x);}
void main(){
 vec3 x=max(texture(beauty,uv*texture_scale).rgb,vec3(0));
 if(enabled!=0){
   x=x*tone.x/max(white,vec3(.0001));
   vec2 p=(uv*2-1)*vec2(max(aspect,1.0),max(1.0/aspect,1.0))*.422793;
   x*=pow(1.0+dot(p,p),-vignette);
   float l=dot(x,vec3(.2126,.7152,.0722));
   x=per_component!=0?compress_color(x):x*(1.0+tone.y*l)/(1.0+l);
   x=mix(x,pow(max(x,vec3(0)),vec3(1.0+2.0*tone.z)),vec3(1)-clamp(x,vec3(0),vec3(1)));
   x=max(mix(vec3(dot(x,vec3(.2126,.7152,.0722))),x,tone.w),vec3(0));
   x=pow(x,vec3(1.0/max(gamma_value,.001)));
 }else{x=mix(12.92*x,1.055*pow(x,vec3(1.0/2.4))-0.055,greaterThan(x,vec3(.0031308)));}
 color=vec4(x,1);
})GLSL");
  program_=glCreateProgram();glAttachShader(program_,v);glAttachShader(program_,f);glLinkProgram(program_);
  glDeleteShader(v);glDeleteShader(f);
  GLint ok;glGetProgramiv(program_,GL_LINK_STATUS,&ok);if(!ok) throw std::runtime_error("显示着色器链接失败");
}
void Display::draw(const Params &) {
  int candidate=-1;
  {
    std::lock_guard lock(slots_mutex_);
    for(int i=0;i<3;++i) if(slots_[i].state==State::ready) {
      const GLenum state=glClientWaitSync(slots_[i].fence,0,0);
      if(state==GL_ALREADY_SIGNALED || state==GL_CONDITION_SATISFIED) {
        if(candidate<0 || slots_[i].frame.id>slots_[candidate].frame.id) candidate=i;
      }
    }
    if(candidate>=0) {
      for(int i=0;i<3;++i) if(i!=candidate && slots_[i].state==State::ready && slots_[i].frame.id<slots_[candidate].frame.id) {
        slots_[i].state=State::retiring;
      }
      if(current_>=0) {
        auto &old=slots_[current_];old.fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);old.state=State::retiring;glFlush();
      }
      auto &next=slots_[candidate];glDeleteSync(next.fence);next.fence=nullptr;next.state=State::displaying;
      current_=candidate;
    }
  }
  if(current_<0) return;
  if(!program_) make_program();
  const auto &slot=slots_[current_];
  glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glUseProgram(program_);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,slot.texture);
  glUniform1i(glGetUniformLocation(program_,"beauty"),0);
  glUniform2f(glGetUniformLocation(program_,"texture_scale"),float(slot.frame.width)/slot.width,float(slot.frame.height)/slot.height);
  const auto &n=options_.tonemapper;const auto w=ir::color(n,"White Point");const auto ws=float(ir::number(n,"White Point Scale",1));
  glUniform1i(glGetUniformLocation(program_,"enabled"),!n.id.empty()&&ir::number(n,"Tone Mapping Enable",1));
  glUniform1i(glGetUniformLocation(program_,"per_component"),int(ir::number(n,"Burn Highlights Per Component",1)));
  glUniform4f(glGetUniformLocation(program_,"tone"),ir::exposure(options_),float(ir::number(n,"Burn Highlights",.25)),float(ir::number(n,"Crush Blacks",.2)),float(ir::number(n,"Saturation",1)));
  glUniform3f(glGetUniformLocation(program_,"white"),w.x*ws,w.y*ws,w.z*ws);
  glUniform1f(glGetUniformLocation(program_,"gamma_value"),float(ir::number(n,"Gamma",2.2)));
  glUniform1f(glGetUniformLocation(program_,"vignette"),float(ir::number(n,"Vignetting",0)));
  glUniform1f(glGetUniformLocation(program_,"aspect"),float(window_.width)/float(window_.height));
  glBegin(GL_QUADS);
  glTexCoord2f(0,0);glVertex2f(-1,-1);glTexCoord2f(1,0);glVertex2f(1,-1);
  glTexCoord2f(1,1);glVertex2f(1,1);glTexCoord2f(0,1);glVertex2f(-1,1);
  glEnd();glUseProgram(0);glBindTexture(GL_TEXTURE_2D,0);
  last_drawn_=slot.frame;
}
void Display::after_swap() {
  if(last_drawn_.id && last_drawn_.id!=last_presented_) {last_presented_=last_drawn_.id;telemetry_.present(last_drawn_);}
}
void Display::hud(const std::string &text) {
  if(!font_) {
    font_=glGenLists(128);
    HFONT font=CreateFontA(-20,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FF_DONTCARE,"Consolas");
    auto previous=SelectObject(window_.dc,font);wglUseFontBitmapsA(window_.dc,0,128,font_);
    SelectObject(window_.dc,previous);DeleteObject(font);
  }
  glUseProgram(0);glDisable(GL_TEXTURE_2D);glDisable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,window_.width,0,window_.height,-1,1);
  glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  glColor3f(0.05f,0.05f,0.05f);glBegin(GL_QUADS);
  glVertex2i(0,window_.height);glVertex2i(window_.width,window_.height);glVertex2i(window_.width,window_.height-35);glVertex2i(0,window_.height-35);glEnd();
  glColor3f(0.9f,0.95f,1);glRasterPos2i(12,window_.height-25);glListBase(font_);glCallLists(GLsizei(text.size()),GL_UNSIGNED_BYTE,text.data());
}
void Display::release_present_resources() {if(program_) glDeleteProgram(program_);if(font_) glDeleteLists(font_,128);program_=font_=0;}
}
