#include "daz/documents.h"
#include "diagnostics/load_profile.h"
#include <zlib.h>
#include <atomic>
#include <fstream>
#include <mutex>
#include <map>
#include <list>
#include <unordered_map>
#include <thread>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace dfv::daz {
using J=nlohmann::json;namespace fs=std::filesystem;
static std::string utf8(const fs::path &p) {auto s=p.generic_u8string();return {s.begin(),s.end()};}
const J &array_member(const J &j,const char *key) {static const J empty=J::array();const auto i=j.find(key);return i==j.end()?empty:*i;}
const J &object_member(const J &j,const char *key) {static const J empty=J::object();const auto i=j.find(key);return i==j.end()?empty:*i;}
std::string file_version(const fs::path &file) {
  return std::to_string(fs::file_size(file))+":"+std::to_string(fs::last_write_time(file).time_since_epoch().count());
}
std::string document_bytes(const fs::path &file) {
  diagnostics::Scope scope("document_bytes");
  constexpr size_t limit=512*1024*1024;
  std::string bytes;
  {diagnostics::Scope read("read_bytes");std::ifstream input(file,std::ios::binary|std::ios::ate);
    if(!input) throw std::runtime_error("无法读取 "+utf8(file));const auto size=input.tellg();
    if(size<0||uint64_t(size)>limit) throw std::runtime_error("文档超过 512 MiB 限制");
    bytes.resize(size_t(size));input.seekg(0);if(!input.read(bytes.data(),std::streamsize(bytes.size()))) throw std::runtime_error("文档读取不完整："+utf8(file));}
  if(bytes.size()<2||uint8_t(bytes[0])!=0x1f||uint8_t(bytes[1])!=0x8b) return bytes;
  diagnostics::Scope inflate_scope("inflate");
  z_stream stream{};stream.next_in=reinterpret_cast<Bytef *>(bytes.data());stream.avail_in=uInt(bytes.size());
  if(inflateInit2(&stream,15+32)!=Z_OK) throw std::runtime_error("gzip 初始化失败");
  struct End {z_stream *s;~End(){inflateEnd(s);}} end{&stream};
  std::string result;std::array<char,65536> chunk{};int code=Z_OK;
  while(code==Z_OK) {
    stream.next_out=reinterpret_cast<Bytef *>(chunk.data());stream.avail_out=uInt(chunk.size());code=inflate(&stream,Z_NO_FLUSH);
    result.append(chunk.data(),chunk.size()-stream.avail_out);if(result.size()>limit) throw std::runtime_error("gzip 解压结果超过限制");
    if(code==Z_STREAM_END&&stream.avail_in) {
      if(stream.avail_in<2||stream.next_in[0]!=0x1f||stream.next_in[1]!=0x8b) break;
      auto *next=stream.next_in;auto remaining=stream.avail_in;code=inflateReset2(&stream,15+32);stream.next_in=next;stream.avail_in=remaining;
    }
  }
  if(code!=Z_STREAM_END||stream.avail_in) throw std::runtime_error("损坏或不支持的 gzip 文档："+utf8(file));return result;
}
namespace {
// SAX 仍验证完整 JSON 语法，但不为被省略的数值数组分配 DOM 节点。
struct MetadataSax : nlohmann::json_sax<J> {
  using InputAdapter=decltype(nlohmann::detail::input_adapter(std::declval<const std::string &>()));
  nlohmann::detail::json_sax_dom_parser<J,InputAdapter> dom;
  DocumentView view;std::vector<std::string> parents;std::string key_name;int skipped=0;bool skip_next=false,count_deltas=false;size_t skipped_items=0;
  MetadataSax(J &out,DocumentView v):dom(out,true),view(v) {}
  template<class F> bool scalar(F f) {if(skipped) {if(skipped==1) ++skipped_items;return true;}if(skip_next) {skip_next=false;return dom.null();}return f();}
  bool null() override{return scalar([&]{return dom.null();});}
  bool boolean(bool v) override{return scalar([&]{return dom.boolean(v);});}
  bool number_integer(number_integer_t v) override{return scalar([&]{return dom.number_integer(v);});}
  bool number_unsigned(number_unsigned_t v) override{return scalar([&]{return dom.number_unsigned(v);});}
  bool number_float(number_float_t v,const string_t &s) override{return scalar([&]{return dom.number_float(v,s);});}
  bool string(string_t &v) override{return scalar([&]{return dom.string(v);});}
  bool binary(binary_t &v) override{return scalar([&]{return dom.binary(v);});}
  bool start(bool object,size_t size) {
    if(skipped) {if(skipped==1) ++skipped_items;++skipped;return true;}
    if(skip_next) {count_deltas=key_name=="values"&&parents.back()=="deltas";skipped_items=0;skip_next=false;skipped=1;dom.start_array(0);return dom.end_array();}
    parents.push_back(std::move(key_name));key_name.clear();return object?dom.start_object(size):dom.start_array(size);
  }
  bool finish(bool object) {if(skipped) {if(!--skipped&&count_deltas) {std::string key="_dfv_rows";dom.key(key);dom.number_unsigned(skipped_items);count_deltas=false;}return true;}parents.pop_back();return object?dom.end_object():dom.end_array();}
  bool start_object(size_t n) override{return start(true,n);}
  bool end_object() override{return finish(true);}
  bool start_array(size_t n) override{return start(false,n);}
  bool end_array() override{return finish(false);}
  bool key(string_t &v) override {
    if(skipped) return true;key_name=v;
    skip_next=(parents.size()==1&&(v=="geometry_library"||v=="uv_set_library"))||
      (v=="values"&&!parents.empty()&&parents.back()=="deltas")||
      (view==DocumentView::metadata&&v=="joints"&&!parents.empty()&&parents.back()=="skin");
    return dom.key(v);
  }
  bool parse_error(size_t p,const std::string &s,const nlohmann::detail::exception &e) override{return dom.parse_error(p,s,e);}
};
struct Entry {std::shared_ptr<const J> value;size_t cost=0;std::list<std::string>::iterator position;};
std::mutex cache_mutex;std::unordered_map<std::string,Entry> cache;std::list<std::string> recent;size_t cache_bytes=0;
constexpr size_t memory_budget=192*1024*1024;
fs::path cache_directory() {
  if(const auto *override_path=std::getenv("DFV_ASSET_CACHE")) return fs::u8path(override_path)/"metadata-v2";
  if(const auto *local=std::getenv("LOCALAPPDATA")) return fs::u8path(local)/"DazFastViewer/cache/metadata-v2";
  return fs::temp_directory_path()/"DazFastViewer/cache/metadata-v2";
}
fs::path cache_path(const std::string &key) {
  uint64_t hash=14695981039346656037ull;for(unsigned char c:key) {hash^=c;hash*=1099511628211ull;}
  std::ostringstream name;name<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;return cache_directory()/(name.str()+".cbor");
}
}
std::shared_ptr<const J> document_view(const fs::path &file,DocumentView view) {
  diagnostics::Scope scope("metadata_document");
  auto path=utf8(fs::absolute(file).lexically_normal());for(auto &c:path) if(c>='A'&&c<='Z') c+=32;
  const auto version=file_version(file);const auto key=path+"|"+std::to_string(int(view))+"|"+version;
  {std::lock_guard lock(cache_mutex);if(auto i=cache.find(key);i!=cache.end()) {recent.splice(recent.end(),recent,i->second.position);return i->second.value;}}
  J document;std::vector<uint8_t> binary;const auto cached=cache_path(path+"|"+std::to_string(int(view)));size_t cache_size=0;
  try {
    if(fs::is_regular_file(cached)&&fs::file_size(cached)<128*1024*1024) {
      cache_size=size_t(fs::file_size(cached));std::ifstream input(cached,std::ios::binary);std::vector<uint8_t> bytes(cache_size);
      if(!input.read(reinterpret_cast<char *>(bytes.data()),std::streamsize(bytes.size()))) throw std::runtime_error("元数据缓存读取不完整");
      auto envelope=J::from_cbor(bytes);
      if(envelope.at("key")==key) document=std::move(envelope.at("document"));
    }
  } catch(...) {document=nullptr;}
  if(document.is_null()) {
    auto bytes=document_bytes(file);MetadataSax sax(document,view);
    if(!J::sax_parse(bytes,&sax)) throw std::runtime_error("资源元数据解析失败："+path);
    if(file_version(file)!=version) throw std::runtime_error("读取期间资源已变化，请刷新参数目录："+path);
    binary=J::to_cbor(J{{"key",key},{"document",document}});
    // 缓存是可丢弃派生数据；写入失败不影响原始资源加载。
    try {fs::create_directories(cached.parent_path());static std::atomic<uint64_t> temp_id=0;
      auto temp=cached;temp+="."+std::to_string(diagnostics::Clock::now().time_since_epoch().count())+"."+std::to_string(++temp_id)+".tmp";
      {std::ofstream out(temp,std::ios::binary);out.write(reinterpret_cast<const char *>(binary.data()),std::streamsize(binary.size()));if(!out) throw std::runtime_error("缓存写入失败");}
      if(!MoveFileExW(temp.c_str(),cached.c_str(),MOVEFILE_REPLACE_EXISTING)) {std::error_code error;fs::remove(temp,error);}
    } catch(...) {}
  }
  auto result=std::make_shared<const J>(std::move(document));
  const size_t cost=(binary.empty()?cache_size:binary.size())*6;
  {std::lock_guard lock(cache_mutex);if(auto i=cache.find(key);i!=cache.end()) return i->second.value;
    recent.push_back(key);cache.emplace(key,Entry{result,cost,std::prev(recent.end())});cache_bytes+=cost;
    while(cache_bytes>memory_budget&&cache.size()>1) {auto oldest=cache.find(recent.front());cache_bytes-=oldest->second.cost;cache.erase(oldest);recent.pop_front();}}
  return result;
}
void prefetch_documents(const std::vector<fs::path> &files) {
  std::atomic<size_t> next=0;std::vector<std::jthread> workers;
  const auto count=std::min<size_t>(4,files.size());
  for(size_t worker=0;worker<count;++worker) workers.emplace_back([&] {for(;;) {auto i=next.fetch_add(1);if(i>=files.size()) break;try {document_view(files[i]);} catch(...) {/* 主线程按稳定顺序报告错误。 */}}});
}
}
