#include "daz/source_archive.h"
#include <zlib.h>
#include <fstream>
#include <array>
#include <algorithm>
#include <cwctype>
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>

namespace dfv::daz {
namespace {
thread_local std::shared_ptr<SourceArchive> active;
constexpr size_t limit=512ull*1024*1024;
std::string read_file(const std::filesystem::path &p) {
  std::ifstream in(p,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("无法读取场景底稿");
  const auto n=in.tellg();if(n<0||uint64_t(n)>limit)throw std::runtime_error("场景底稿超过大小限制");
  std::string bytes(size_t(n),'\0');in.seekg(0);if(!in.read(bytes.data(),std::streamsize(bytes.size())))throw std::runtime_error("场景底稿读取不完整");return bytes;
}
std::shared_ptr<FrozenDocument> freeze(const std::string &body) {
  auto d=std::make_shared<FrozenDocument>();d->digest=source_digest(body);d->size=body.size();const auto packed=gzip_document(body);d->compressed.assign(packed.begin(),packed.end());return d;
}
}
std::string source_key(const std::filesystem::path &p){auto u=std::filesystem::absolute(p).lexically_normal().generic_u8string();std::string s(u.begin(),u.end());for(auto &c:s)if(c>='A'&&c<='Z')c+=32;return s;}
std::string source_digest(const std::string &body) {
  HCRYPTPROV provider=0;HCRYPTHASH hash=0;
  if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))throw std::runtime_error("无法校验场景底稿");
  struct Release{HCRYPTPROV p;HCRYPTHASH &h;~Release(){if(h)CryptDestroyHash(h);CryptReleaseContext(p,0);}} release{provider,hash};
  if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)||!CryptHashData(hash,reinterpret_cast<const BYTE *>(body.data()),DWORD(body.size()),0))throw std::runtime_error("场景底稿校验失败");
  std::array<BYTE,32> bytes;DWORD count=DWORD(bytes.size());if(!CryptGetHashParam(hash,HP_HASHVAL,bytes.data(),&count,0))throw std::runtime_error("场景底稿校验失败");
  std::string result;for(auto b:bytes){result+="0123456789abcdef"[b>>4];result+="0123456789abcdef"[b&15];}return result;
}
std::string gzip_document(const std::string &body) {
  if(body.size()>limit)throw std::runtime_error("场景文档超过大小限制");
  z_stream s{};if(deflateInit2(&s,Z_DEFAULT_COMPRESSION,Z_DEFLATED,15+16,8,Z_DEFAULT_STRATEGY)!=Z_OK)throw std::runtime_error("场景压缩初始化失败");
  struct End{z_stream *s;~End(){deflateEnd(s);}} end{&s};s.next_in=reinterpret_cast<Bytef *>(const_cast<char *>(body.data()));s.avail_in=uInt(body.size());
  std::array<char,65536> buffer;std::string out;int code;do{s.next_out=reinterpret_cast<Bytef *>(buffer.data());s.avail_out=uInt(buffer.size());code=deflate(&s,Z_FINISH);if(code!=Z_OK&&code!=Z_STREAM_END)throw std::runtime_error("场景压缩失败");out.append(buffer.data(),buffer.size()-s.avail_out);}while(code!=Z_STREAM_END);return out;
}
std::string unpack_document(const std::string &body) {
  if(body.size()>limit)throw std::runtime_error("场景文档超过大小限制");
  if(body.size()<2||uint8_t(body[0])!=0x1f||uint8_t(body[1])!=0x8b)return body;
  z_stream s{};if(inflateInit2(&s,15+32)!=Z_OK)throw std::runtime_error("场景解压初始化失败");
  struct End{z_stream *s;~End(){inflateEnd(s);}} end{&s};s.next_in=reinterpret_cast<Bytef *>(const_cast<char *>(body.data()));s.avail_in=uInt(body.size());
  std::array<char,65536> buffer;std::string out;int code=Z_OK;while(code==Z_OK){s.next_out=reinterpret_cast<Bytef *>(buffer.data());s.avail_out=uInt(buffer.size());code=inflate(&s,Z_NO_FLUSH);out.append(buffer.data(),buffer.size()-s.avail_out);if(out.size()>limit)throw std::runtime_error("场景解压结果超过大小限制");if(code==Z_STREAM_END&&s.avail_in>=2&&s.next_in[0]==0x1f&&s.next_in[1]==0x8b){auto *next=s.next_in;auto remaining=s.avail_in;code=inflateReset2(&s,15+32);s.next_in=next;s.avail_in=remaining;}}
  if(code!=Z_STREAM_END||s.avail_in)throw std::runtime_error("场景压缩数据损坏");return out;
}
std::string FrozenDocument::bytes() const {return unpack_document(std::string(compressed.begin(),compressed.end()));}
std::shared_ptr<const FrozenDocument> SourceArchive::find(const std::filesystem::path &p) const {std::lock_guard lock(mutex_);const auto i=documents_.find(source_key(p));return i==documents_.end()?nullptr:i->second;}
std::shared_ptr<const FrozenDocument> SourceArchive::capture(const std::filesystem::path &p) {
  if(auto d=find(p))return d;
  const auto version=std::make_pair(std::filesystem::file_size(p),std::filesystem::last_write_time(p));auto d=freeze(unpack_document(read_file(p)));
  if(version!=std::make_pair(std::filesystem::file_size(p),std::filesystem::last_write_time(p)))throw std::runtime_error("读取期间场景底稿已变化");
  std::lock_guard lock(mutex_);return documents_.emplace(source_key(p),std::move(d)).first->second;
}
nlohmann::json SourceArchive::json() const {std::lock_guard lock(mutex_);auto out=nlohmann::json::object();for(const auto &[p,d]:documents_)out[p]={{"sha256",d->digest},{"document",d->bytes()}};return out;}
std::string SourceArchive::identity() const {std::lock_guard lock(mutex_);auto manifest=nlohmann::json::object();for(const auto &[p,d]:documents_)manifest[p]=d->digest;return source_digest(manifest.dump());}
std::shared_ptr<SourceArchive> SourceArchive::read(const nlohmann::json &j) {
  if(!j.is_object())throw std::runtime_error("场景底稿目录无效");auto result=std::make_shared<SourceArchive>();size_t total=0;
  for(const auto &[p,v]:j.items()){const auto path=std::filesystem::u8path(p);if(!path.is_absolute())throw std::runtime_error("场景底稿需要完整的来源地址");const auto body=v.at("document").get<std::string>();total+=body.size();if(total>limit)throw std::runtime_error("场景底稿超过大小限制");auto d=freeze(body);if(d->digest!=v.at("sha256").get<std::string>())throw std::runtime_error("场景底稿校验不一致");if(!result->documents_.emplace(source_key(path),std::move(d)).second)throw std::runtime_error("场景底稿地址重复");}return result;
}
DocumentScope::DocumentScope(std::shared_ptr<SourceArchive> a):previous_(std::move(active)){active=std::move(a);}
DocumentScope::~DocumentScope(){active=std::move(previous_);}
std::shared_ptr<SourceArchive> current_archive(){return active;}
std::shared_ptr<const FrozenDocument> frozen_document(const std::filesystem::path &p,bool capture){if(!active)return {};if(auto d=active->find(p))return d;auto ext=p.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);return capture&&ext==L".duf"?active->capture(p):nullptr;}
bool document_exists(const std::filesystem::path &p){if(frozen_document(p))return true;std::error_code ec;return std::filesystem::is_regular_file(p,ec);}
}
