#include "modhost.h"
#include "modutil.h"

#include <cctype>
#include <cstring>
#include <map>

#include <miniz.h>

namespace fs = std::filesystem;

using namespace Scripting;

namespace {

class DirSource final : public ModSource {
  public:
    explicit DirSource(fs::path root):root(std::move(root)) {}

    bool read(std::string_view path, std::string& out) const override {
      std::string rel;
      if(!normalizePath(path,rel))
        return false;
      return readFile(root/toPath(rel),out);
      }

    bool exists(std::string_view path) const override {
      std::string rel;
      if(!normalizePath(path,rel))
        return false;
      std::error_code ec;
      return fs::is_regular_file(root/toPath(rel),ec);
      }

  private:
    fs::path root;
  };

/// The whole archive is read to the memory, so the file can be replaced or deleted while the mod works
class ZipSource final : public ModSource {
  public:
    static std::shared_ptr<ZipSource> open(const fs::path& file) {
      auto ret = std::make_shared<ZipSource>();
      if(!readFile(file,ret->bytes))
        return nullptr;

      mz_zip_archive zip;
      std::memset(&zip,0,sizeof(zip));
      if(!mz_zip_reader_init_mem(&zip,ret->bytes.data(),ret->bytes.size(),0))
        return nullptr;

      // manifest.json is in the root of the archive, or in the only folder of it
      std::string prefix;
      bool        found = false;
      for(mz_uint i=0; i<mz_zip_reader_get_num_files(&zip); ++i) {
        char name[512] = {};
        mz_zip_reader_get_filename(&zip,i,name,sizeof(name));
        const std::string n = name;
        if(n=="manifest.json") {
          prefix = "";
          found  = true;
          break;
          }
        const auto slash = n.find('/');
        if(!found && slash!=std::string::npos && n.substr(slash+1)=="manifest.json") {
          prefix = n.substr(0,slash+1);
          found  = true;
          }
        }
      mz_zip_reader_end(&zip);
      if(!found)
        return nullptr;
      ret->prefix = prefix;
      return ret;
      }

    bool read(std::string_view path, std::string& out) const override {
      std::string rel;
      if(!normalizePath(path,rel))
        return false;

      mz_zip_archive zip;
      std::memset(&zip,0,sizeof(zip));
      if(!mz_zip_reader_init_mem(&zip,bytes.data(),bytes.size(),0))
        return false;

      bool   ok   = false;
      size_t size = 0;
      void*  data = mz_zip_reader_extract_file_to_heap(&zip,(prefix+rel).c_str(),&size,0);
      if(data!=nullptr) {
        out.assign(static_cast<const char*>(data),size);
        mz_free(data);
        ok = true;
        }
      mz_zip_reader_end(&zip);
      return ok;
      }

    bool exists(std::string_view path) const override {
      std::string rel;
      if(!normalizePath(path,rel))
        return false;

      mz_zip_archive zip;
      std::memset(&zip,0,sizeof(zip));
      if(!mz_zip_reader_init_mem(&zip,bytes.data(),bytes.size(),0))
        return false;
      const bool ok = mz_zip_reader_locate_file(&zip,(prefix+rel).c_str(),nullptr,0)>=0;
      mz_zip_reader_end(&zip);
      return ok;
      }

  private:
    std::string bytes;
    std::string prefix;
  };

class MemorySource final : public ModSource {
  public:
    MemorySource(const BuiltinFile* files, size_t count) {
      for(size_t i=0; i<count; ++i)
        data[files[i].path].assign(reinterpret_cast<const char*>(files[i].data),files[i].size);
      }

    bool read(std::string_view path, std::string& out) const override {
      std::string rel;
      if(!normalizePath(path,rel))
        return false;
      auto it = data.find(rel);
      if(it==data.end())
        return false;
      out = it->second;
      return true;
      }

    bool exists(std::string_view path) const override {
      std::string rel;
      return normalizePath(path,rel) && data.find(rel)!=data.end();
      }

  private:
    std::map<std::string,std::string> data;
  };

bool validId(const std::string& id) {
  if(id.empty() || id.size()>64)
    return false;
  for(char c:id) {
    if(!(std::isalnum(static_cast<unsigned char>(c)) || c=='_' || c=='-' || c=='.'))
      return false;
    }
  return true;
  }

}

std::shared_ptr<ModSource> ModSource::memory(const BuiltinFile* files, size_t count) {
  return std::make_shared<MemorySource>(files,count);
  }

std::shared_ptr<ModSource> ModSource::open(const fs::path& path) {
  std::error_code ec;
  if(fs::is_directory(path,ec)) {
    if(!fs::is_regular_file(path/"manifest.json",ec))
      return nullptr;
    return std::make_shared<DirSource>(path);
    }
  if(fs::is_regular_file(path,ec) && lowerCase(toUtf8(path.extension()))==".zip")
    return ZipSource::open(path);
  return nullptr;
  }

bool Manifest::parse(std::string_view json, Manifest& out, std::string& error) {
  JsonReader r(json);
  if(!r.ok()) {
    error = "manifest.json is not a valid JSON object";
    return false;
    }

  r.string("id",out.id);
  r.string("name",out.name);
  r.string("version",out.version);
  r.string("author",out.author);
  r.string("description",out.description);
  r.string("main",out.main);
  r.integer("apiVersion",out.apiVersion);

  std::string type;
  r.string("type",type);
  out.module = type=="module";
  out.game   = r.strings("game");

  if(out.name.empty())
    out.name = out.id;
  if(!validId(out.id)) {
    error = "manifest.json needs an id of letters, digits, '_', '-' and '.'";
    return false;
    }
  return true;
  }
