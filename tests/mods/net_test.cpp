// Host-side test of net: two mods talk to each other over the loopback. See tests/mods/README.md for the command.
#include "scripting/modhost.h"

#include <chrono>
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

bool logged(ModHost& h, const std::string& text) {
  for(auto& l:h.lines()) {
    if(l.text.find(text)!=std::string::npos)
      return true;
    }
  return false;
  }

/// Runs the frames until the text is in the log
bool waitFor(ModHost& h, const std::string& text, uint64_t& clock) {
  const auto t0 = std::chrono::steady_clock::now();
  while(std::chrono::steady_clock::now()-t0<std::chrono::seconds(5)) {
    clock += 10;
    h.tick(clock);
    if(logged(h,text))
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  return false;
  }

}

int main() {
  const fs::path root = fs::temp_directory_path()/"og-net-test";
  std::error_code ec;
  fs::remove_all(root,ec);

  write(root/"mods"/"srv"/"manifest.json",R"({"id":"srv"})");
  write(root/"mods"/"srv"/"index.js",R"(
    const port = net.listen(0);
    console.log("PORT " + port);
    console.log("listening " + net.listening);
    events.on("net:connect", ({peer, incoming}) => console.log("SRV_CONNECT " + peer + " " + incoming));
    events.on("net:message", ({peer, data}) => {
      if(data.length > 1000) { net.send(peer, "big " + data.length); return; }
      net.send(peer, "echo:" + data);
    });
    events.on("net:disconnect", ({peer}) => console.log("SRV_DISC " + peer));
    try { net.listen(0); console.log("LISTEN_TWICE"); } catch(e) { console.log("twice refused"); }
  )");

  auto& host = ModHost::inst();
  host.setGame("G2");
  host.setEnabledByDefault(false);
  host.setDataDir(root/"data");
  host.addRoot(root/"mods");
  host.scan();
  CHECK(host.start(*host.find("srv")));
  CHECK(logged(host,"listening true"));
  CHECK(logged(host,"twice refused"));

  // the port that the server has taken
  int port = 0;
  for(auto& l:host.lines()) {
    if(l.text.rfind("PORT ",0)==0)
      port = std::atoi(l.text.c_str()+5);
    }
  CHECK(port>0);

  write(root/"mods"/"cli"/"manifest.json",R"({"id":"cli"})");
  write(root/"mods"/"cli"/"index.js","const PORT = "+std::to_string(port)+R"(;
    const p = net.connect("127.0.0.1", PORT);
    console.log("peers before " + net.peers.length);
    events.on("net:connect", ({peer, incoming, address}) => {
      console.log("CLI_CONNECT " + peer + " " + incoming + " " + address);
      net.send(peer, {hello: "world"});
    });
    events.on("net:message", ({peer, data}) => {
      console.log("GOT " + data);
      if(data.startsWith("echo:")) net.send(peer, "x".repeat(1000000));
      else net.disconnect(peer);
    });
    try { net.connect("127.0.0.1", 0); } catch(e) { console.log("bad port refused"); }
  )");
  host.scan();
  CHECK(host.start(*host.find("cli")));
  CHECK(logged(host,"bad port refused"));

  uint64_t clock = 1000;
  CHECK(waitFor(host,"SRV_CONNECT 1 true",clock));
  CHECK(waitFor(host,"CLI_CONNECT 1 false 127.0.0.1:"+std::to_string(port),clock));
  CHECK(waitFor(host,"GOT echo:{\"hello\":\"world\"}",clock));
  CHECK(waitFor(host,"GOT big 1000000",clock));
  CHECK(waitFor(host,"SRV_DISC 1",clock));

  // a mod that stops gives the port back
  host.stop(*host.find("srv"));
  host.stop(*host.find("cli"));
  write(root/"mods"/"again"/"manifest.json",R"({"id":"again"})");
  write(root/"mods"/"again"/"index.js","net.listen("+std::to_string(port)+"); console.log('REUSED');");
  host.scan();
  CHECK(host.start(*host.find("again")));
  CHECK(logged(host,"REUSED"));

  // a connection that cannot be made reports the error
  write(root/"mods"/"lost"/"manifest.json",R"({"id":"lost"})");
  write(root/"mods"/"lost"/"index.js",R"(
    net.connect("127.0.0.1", 1);
    events.on("net:error", ({error}) => console.log("LOST_ERROR"));
  )");
  host.scan();
  CHECK(host.start(*host.find("lost")));
  CHECK(waitFor(host,"LOST_ERROR",clock));

  host.stopAll();
  std::printf("PASS\n");
  fs::remove_all(root,ec);
  return 0;
  }
