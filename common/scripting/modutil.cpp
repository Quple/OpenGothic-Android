#include "modutil.h"

#include <cctype>
#include <chrono>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace Scripting {

fs::path toPath(std::string_view s) {
  const std::u8string u(reinterpret_cast<const char8_t*>(s.data()),s.size());
  return fs::path(u);
  }

std::string toUtf8(const fs::path& p) {
  const auto u = p.u8string();
  return std::string(reinterpret_cast<const char*>(u.data()),u.size());
  }

std::string lowerCase(std::string s) {
  for(auto& c:s)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
  }

uint64_t steadyMs() {
  using namespace std::chrono;
  return uint64_t(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
  }

bool normalizePath(std::string_view path, std::string& out) {
  std::vector<std::string> parts;
  std::string cur;
  auto flush = [&]() {
    if(cur.empty() || cur==".") {
      cur.clear();
      return true;
      }
    if(cur=="..") {
      if(parts.empty())
        return false;
      parts.pop_back();
      } else {
      parts.push_back(cur);
      }
    cur.clear();
    return true;
    };

  for(char c:path) {
    if(c=='/' || c=='\\') {
      if(!flush())
        return false;
      } else {
      cur.push_back(c);
      }
    }
  if(!flush())
    return false;

  out.clear();
  for(size_t i=0; i<parts.size(); ++i) {
    if(i>0)
      out += '/';
    out += parts[i];
    }
  return !out.empty();
  }

std::string dirName(std::string_view path) {
  const auto at = path.find_last_of('/');
  return at==std::string_view::npos ? std::string() : std::string(path.substr(0,at));
  }

bool readFile(const fs::path& path, std::string& out) {
  std::ifstream in(path,std::ios::binary);
  if(!in)
    return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
  }

bool writeFile(const fs::path& path, std::string_view data) {
  std::error_code ec;
  fs::create_directories(path.parent_path(),ec);
  std::ofstream out(path,std::ios::binary|std::ios::trunc);
  if(!out)
    return false;
  out.write(data.data(),std::streamsize(data.size()));
  return bool(out);
  }

JsonReader::JsonReader(std::string_view json) {
  rt = JS_NewRuntime();
  if(rt==nullptr)
    return;
  ctx = JS_NewContext(rt);
  if(ctx==nullptr)
    return;
  root = JS_ParseJSON(ctx,json.data(),json.size(),"json");
  if(JS_IsException(root)) {
    JS_FreeValue(ctx,JS_GetException(ctx));
    root = JS_UNDEFINED;
    }
  }

JsonReader::~JsonReader() {
  if(ctx!=nullptr) {
    JS_FreeValue(ctx,root);
    JS_FreeContext(ctx);
    }
  if(rt!=nullptr)
    JS_FreeRuntime(rt);
  }

bool JsonReader::string(const char* key, std::string& out) const {
  if(!ok())
    return false;
  JSValue v = JS_GetPropertyStr(ctx,root,key);
  bool    r = false;
  if(JS_IsString(v)) {
    if(const char* s = JS_ToCString(ctx,v)) {
      out = s;
      JS_FreeCString(ctx,s);
      r = true;
      }
    }
  JS_FreeValue(ctx,v);
  return r;
  }

bool JsonReader::integer(const char* key, int& out) const {
  if(!ok())
    return false;
  JSValue v = JS_GetPropertyStr(ctx,root,key);
  bool    r = false;
  if(JS_IsNumber(v)) {
    int32_t n = 0;
    r = JS_ToInt32(ctx,&n,v)==0;
    out = n;
    } else
  if(JS_IsString(v)) {
    if(const char* s = JS_ToCString(ctx,v)) {
      out = std::atoi(s);
      JS_FreeCString(ctx,s);
      r = true;
      }
    }
  JS_FreeValue(ctx,v);
  return r;
  }

std::vector<std::string> JsonReader::strings(const char* key) const {
  std::vector<std::string> ret;
  if(!ok())
    return ret;

  auto add = [&](JSValueConst v) {
    if(!JS_IsString(v))
      return;
    if(const char* s = JS_ToCString(ctx,v)) {
      ret.push_back(s);
      JS_FreeCString(ctx,s);
      }
    };

  JSValue v = JS_GetPropertyStr(ctx,root,key);
  if(JS_IsString(v)) {
    add(v);
    } else
  if(JS_IsArray(v)) {
    JSValue len = JS_GetPropertyStr(ctx,v,"length");
    uint32_t n  = 0;
    JS_ToUint32(ctx,&n,len);
    JS_FreeValue(ctx,len);
    for(uint32_t i=0; i<n; ++i) {
      JSValue e = JS_GetPropertyUint32(ctx,v,i);
      add(e);
      JS_FreeValue(ctx,e);
      }
    }
  JS_FreeValue(ctx,v);
  return ret;
  }

}
