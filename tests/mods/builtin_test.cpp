// Host-side test of the mods that come with the program (ModHost::addBuiltin) and of net.localAddresses.
// See tests/mods/README.md for the command.
#include "scripting/modhost.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace fs = std::filesystem;
using namespace Scripting;

#define CHECK(cond) \
  do { \
    if(!(cond)) { \
      std::fprintf(stderr,"%s:%d: check failed: %s\n",__FILE__,__LINE__,#cond); \
      for(auto& l:ModHost::inst().lines()) \
        std::fprintf(stderr,"  [%s] %s\n",l.source.c_str(),l.text.c_str()); \
      std::abort(); \
      } \
    } while(false)

namespace {

void write(const fs::path& file, const std::string& text) {
  fs::create_directories(file.parent_path());
  std::ofstream(file,std::ios::binary) << text;
  }

bool logged(ModHost& h, const std::string& text) {
  for(auto& l:h.lines())
    if(l.text.find(text)!=std::string::npos)
      return true;
  return false;
  }

BuiltinFile file(const char* path, const std::string& text) {
  return {path,reinterpret_cast<const unsigned char*>(text.data()),text.size()};
  }

}

int main() {
  const fs::path root = fs::temp_directory_path()/"og-builtin-test";
  std::error_code ec;
  fs::remove_all(root,ec);
  fs::create_directories(root/"mods");

  auto& host = ModHost::inst();
  host.setGame("G2");
  host.setEnabledByDefault(false);
  host.setDataDir(root/"data");
  host.addRoot(root/"mods");

  // the files of a mod that is a part of the program
  static const std::string manifest = R"({"id":"embedded","name":"Embedded","version":"2.0.0"})";
  static const std::string index    = "console.log('embedded runs ' + require('./lib/x.js').value + ' ' + net.localAddresses().length);";
  static const std::string lib      = "module.exports = { value: 7 };";
  static const BuiltinFile files[]  = {file("manifest.json",manifest),file("index.js",index),file("lib/x.js",lib)};
  host.addBuiltin("embedded",files,3);
  host.scan();

  CHECK(host.mods().size()==1);
  Mod* m = host.find("embedded");
  CHECK(m!=nullptr);
  CHECK(m->manifest.version=="2.0.0");
  CHECK(m->enabled);                 // a mod of the program is on by default
  CHECK(host.start(*m));
  CHECK(logged(host,"embedded runs 7"));

  // the addresses of the computer: a list of texts that are not the loopback
  write(root/"mods"/"addr"/"manifest.json",R"({"id":"addr"})");
  write(root/"mods"/"addr"/"index.js",R"(
    const a = net.localAddresses();
    console.log("ADDRESSES " + Array.isArray(a) + " " + a.every(x => typeof x === "string" && !x.startsWith("127.")));
  )");
  host.scan();
  CHECK(host.start(*host.find("addr")));
  CHECK(logged(host,"ADDRESSES true true"));

  // a scan does not lose it, and does not make it twice
  host.scan();
  CHECK(host.mods().size()==2);
  CHECK(host.find("embedded")->isRunning());

  // a mod of the same id in the folder of the mods replaces it
  write(root/"mods"/"embedded"/"manifest.json",R"({"id":"embedded","name":"Newer","version":"3.0.0"})");
  write(root/"mods"/"embedded"/"index.js","console.log('the newer one runs');");
  host.scan();
  CHECK(host.mods().size()==2);
  m = host.find("embedded");
  CHECK(m!=nullptr);
  CHECK(m->manifest.version=="3.0.0");
  CHECK(!m->isRunning());
  CHECK(host.start(*m));
  CHECK(logged(host,"the newer one runs"));

  host.stopAll();
  std::printf("PASS\n");
  fs::remove_all(root,ec);
  return 0;
  }
