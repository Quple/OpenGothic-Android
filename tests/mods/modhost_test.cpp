// Host-side test of the JavaScript mods. It does not need the game:
//   see tests/mods/README.md for the command
#include "scripting/modhost.h"

#include <miniz.h>

#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
using namespace Scripting;

#define CHECK(cond) \
  do { \
    if(!(cond)) { \
      std::fprintf(stderr,"%s:%d: check failed: %s\n",__FILE__,__LINE__,#cond); \
      std::abort(); \
      } \
    } while(false)

namespace {

void write(const fs::path& file, const std::string& text) {
  fs::create_directories(file.parent_path());
  std::ofstream(file,std::ios::binary) << text;
  }

void writeZip(const fs::path& file, const std::vector<std::pair<std::string,std::string>>& files) {
  mz_zip_archive zip;
  std::memset(&zip,0,sizeof(zip));
  std::string bytes;
  CHECK(mz_zip_writer_init_heap(&zip,0,0));
  for(auto& f:files)
    CHECK(mz_zip_writer_add_mem(&zip,f.first.c_str(),f.second.data(),f.second.size(),MZ_DEFAULT_COMPRESSION));
  void*  data = nullptr;
  size_t size = 0;
  CHECK(mz_zip_writer_finalize_heap_archive(&zip,&data,&size));
  fs::create_directories(file.parent_path());
  std::ofstream(file,std::ios::binary).write(static_cast<const char*>(data),std::streamsize(size));
  mz_zip_writer_end(&zip);
  mz_free(data);
  }

bool logged(ModHost& h, const std::string& text) {
  for(auto& l:h.lines()) {
    if(l.text.find(text)!=std::string::npos)
      return true;
    }
  return false;
  }

}

int main() {
  const fs::path root = fs::temp_directory_path()/"og-mods-test";
  std::error_code ec;
  fs::remove_all(root,ec);

  write(root/"mods"/"alpha"/"manifest.json",
        R"({"id":"alpha","name":"Alpha","version":"1.2.3","author":"me","main":"index.js"})");
  write(root/"mods"/"alpha"/"index.js", R"(
    globalThis.shared = 1;
    const lib = require("./lib/util.js");
    console.log("alpha says " + lib.twice(21));
    events.on("ready", () => console.log("alpha ready " + GothicJS.mod.id));
    events.on("custom", (d) => console.log("custom " + d.value));
    events.once("custom", () => console.log("only once"));
    events.on("shutdown", () => console.log("alpha bye"));
    console.registerCommand("hello", ({args, raw}) => console.log("hello " + args.join("+") + " [" + raw + "]"), "greets");
    storage.set("runs", storage.get("runs", 0) + 1);
    setTimeout((a, b) => console.log("timeout " + a + b), 50, "x", "y");
    let n = 0;
    const id = setInterval(() => { n++; console.log("tick " + n); if(n == 2) clearInterval(id); }, 20);
    Promise.resolve().then(() => console.log("promise ran"));
  )");
  write(root/"mods"/"alpha"/"lib"/"util.js", "module.exports = { twice: (x) => x * 2 };");

  // a mod in a zip, in a folder of the archive, written as a module
  writeZip(root/"mods"/"beta.zip", {
    {"beta-mod/manifest.json", R"({"id":"beta","name":"Beta","type":"module","main":"main.mjs"})"},
    {"beta-mod/main.mjs", "import {name} from './dep.mjs'; console.log('module ' + name + ' ' + typeof shared);"},
    {"beta-mod/dep.mjs", "export const name = 'beta-dep';"}
    });

  write(root/"mods"/"loop"/"manifest.json", R"({"id":"loop"})");
  write(root/"mods"/"loop"/"index.js", "while(true){}");

  write(root/"mods"/"broken"/"manifest.json", R"({"id":"broken"})");
  write(root/"mods"/"broken"/"index.js", "throw new Error('boom');");

  write(root/"mods"/"other"/"manifest.json", R"({"id":"other","game":["G1"]})");
  write(root/"mods"/"other"/"index.js", "console.log('should not run');");

  write(root/"mods"/"escape"/"manifest.json", R"({"id":"escape"})");
  write(root/"mods"/"escape"/"index.js", "try { require('../../alpha/index.js'); console.log('ESCAPED'); } catch(e) { console.log('contained'); }");

  auto& host = ModHost::inst();
  host.setGame("G2");
  host.setEnabledByDefault(false);
  host.setDataDir(root/"data");
  host.addRoot(root/"mods");
  int bound = 0;
  host.binder = [&](JSContext* ctx, Mod&) {
    ++bound;
    JSValue g = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx,g,"answer",JS_NewInt32(ctx,42));
    JS_FreeValue(ctx,g);
    };
  host.scan();
  CHECK(host.mods().size()==6);
  for(auto& m:host.mods())
    CHECK(!m->enabled);

  // starting
  CHECK(host.start(*host.find("alpha")));
  CHECK(host.find("alpha")->isRunning());
  CHECK(logged(host,"alpha says 42"));
  CHECK(logged(host,"alpha ready alpha"));
  CHECK(logged(host,"promise ran"));
  CHECK(bound==1);

  CHECK(host.start(*host.find("beta")));
  CHECK(logged(host,"module beta-dep undefined"));   // modules do not see each other

  // failures do not touch the others
  CHECK(!host.start(*host.find("broken")));
  CHECK(host.find("broken")->state==Mod::State::Failed);
  CHECK(logged(host,"boom"));
  const auto t0 = std::chrono::steady_clock::now();
  CHECK(!host.start(*host.find("loop")));
  CHECK(std::chrono::steady_clock::now()-t0<std::chrono::seconds(10));
  CHECK(host.find("alpha")->isRunning());
  CHECK(!host.start(*host.find("other")));
  CHECK(!logged(host,"should not run"));

  CHECK(host.start(*host.find("escape")));
  CHECK(logged(host,"contained"));
  CHECK(!logged(host,"ESCAPED"));

  // events and commands
  host.emit("custom",[](JSContext* ctx) {
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx,o,"value",JS_NewInt32(ctx,7));
    return o;
    });
  CHECK(logged(host,"custom 7"));
  CHECK(logged(host,"only once"));
  host.clearLog();
  host.emit("custom",[](JSContext* ctx) {
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx,o,"value",JS_NewInt32(ctx,8));
    return o;
    });
  CHECK(logged(host,"custom 8"));
  CHECK(!logged(host,"only once"));

  host.execute("hello a \"b c\" d");
  CHECK(logged(host,"hello a+b c+d [a \"b c\" d]"));
  host.execute("alpha.hello x");
  CHECK(logged(host,"hello x"));
  host.execute("nonsense");
  CHECK(logged(host,"unknown command"));
  host.clearLog();
  host.execute("eval alpha shared + 1");
  CHECK(host.lines().back().text=="2");

  // timers
  uint64_t t = host.nowMs();
  host.tick(t);
  host.tick(t += 25);
  CHECK(logged(host,"tick 1"));
  host.tick(t += 25);
  CHECK(logged(host,"timeout xy"));
  CHECK(logged(host,"tick 2"));
  host.clearLog();
  host.tick(t += 200);
  CHECK(!logged(host,"tick 3"));

  // reset: a new context and a new life
  CHECK(host.reset(*host.find("alpha")));
  CHECK(host.find("alpha")->isRunning());
  CHECK(logged(host,"alpha bye"));
  host.clearLog();
  host.execute("eval alpha typeof shared");
  CHECK(host.lines().back().text=="number");

  // storage survives the reset and a new start
  host.stop(*host.find("alpha"));
  CHECK(!host.find("alpha")->isRunning());
  CHECK(host.start(*host.find("alpha")));
  host.clearLog();
  host.execute("eval alpha storage.get('runs')");
  CHECK(host.lines().back().text=="3");

  // the state of the mods is remembered
  host.setEnabled(*host.find("alpha"),true);
  CHECK(fs::exists(root/"data"/"mods.json"));
  host.stopAll();
  CHECK(host.runningCount()==0);

  std::printf("PASS\n");
  fs::remove_all(root,ec);
  return 0;
  }
