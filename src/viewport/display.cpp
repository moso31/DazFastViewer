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
  window_.render_context.deactivate();
}
void Display::allocate() {
  if(pbo_) return;
  glGenBuffers(1,&pbo_);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo_);
  glBufferData(GL_PIXEL_UNPACK_BUFFER,size_t(window_.width)*window_.height*sizeof(ccl::half4),nullptr,GL_DYNAMIC_DRAW);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
  for(auto &s:slots_) {
    glGenTextures(1,&s.texture);glBindTexture(GL_TEXTURE_2D,s.texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,window_.width,window_.height,0,GL_RGBA,GL_HALF_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  }
  glBindTexture(GL_TEXTURE_2D,0);
  if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("分配 OpenGL 输出资源失败");
}
bool Display::update_begin(const Params &p,int width,int height) {
  if(width!=window_.width || height!=window_.height || p.size.x!=width || p.size.y!=height) {
    error_="渲染尺寸不等于原生 framebuffer";failed_=true;return false;
  }
  window_.render_context.activate();
  allocate();
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
  }
  if(writing_<0) {telemetry_.skipped++;window_.render_context.deactivate();return false;}
  if(upload_) {glWaitSync(upload_,0,GL_TIMEOUT_IGNORED);glDeleteSync(upload_);upload_=nullptr;}
  auto &s=slots_[writing_];s.frame={telemetry_.produced.fetch_add(1)+1,epoch_.load(),samples_.load(),width,height,now()};
  return true;
}
void Display::update_end() {
  auto &s=slots_[writing_];
  glBindTexture(GL_TEXTURE_2D,s.texture);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo_);
  glTexSubImage2D(GL_TEXTURE_2D,0,0,0,window_.width,window_.height,GL_RGBA,GL_HALF_FLOAT,nullptr);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glBindTexture(GL_TEXTURE_2D,0);
  upload_=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
  {
    std::lock_guard lock(slots_mutex_);
    s.fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);s.state=State::ready;
  }
  glFlush();telemetry_.event("produced",s.frame);
  writing_=-1;window_.render_context.deactivate();
}
ccl::half4 *Display::map_texture_buffer() {
  if(!allow_readback_) {error_="GPU interop 不可用；显式 --allow-readback 才允许诊断回读";failed_=true;return nullptr;}
  telemetry_.readback_bytes+=size_t(window_.width)*window_.height*sizeof(ccl::half4);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo_);
  return static_cast<ccl::half4 *>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER,GL_WRITE_ONLY));
}
void Display::unmap_texture_buffer() {glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);}
ccl::GraphicsInteropDevice Display::graphics_interop_get_device() {
  ccl::GraphicsInteropDevice device;device.type=ccl::GraphicsInteropDevice::OPENGL;return device;
}
void Display::graphics_interop_update_buffer() {
  if(graphics_interop_buffer_.is_empty())
    graphics_interop_buffer_.assign(ccl::GraphicsInteropDevice::OPENGL,pbo_,size_t(window_.width)*window_.height*sizeof(ccl::half4));
}
static GLuint shader(GLenum type,const char *source) {
  const GLuint id=glCreateShader(type);glShaderSource(id,1,&source,nullptr);glCompileShader(id);
  GLint ok;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
  if(!ok) {char log[2048];glGetShaderInfoLog(id,2048,nullptr,log);throw std::runtime_error(log);}
  return id;
}
void Display::make_program() {
  const auto v=shader(GL_VERTEX_SHADER,"#version 130\nout vec2 uv;void main(){gl_Position=gl_Vertex;uv=gl_MultiTexCoord0.xy;}");
  const auto f=shader(GL_FRAGMENT_SHADER,"#version 130\nuniform sampler2D beauty;in vec2 uv;out vec4 color;void main(){vec3 x=max(texture(beauty,uv).rgb,vec3(0));vec3 s=mix(12.92*x,1.055*pow(x,vec3(1.0/2.4))-0.055,greaterThan(x,vec3(0.0031308)));color=vec4(s,1);}");
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
