// Host-side test of the developer tools (mods/devtools): the tab Debug is drawn on a pretended ui and its buttons are
// pressed, the game is pretended by tests/mods/fakedebug.js. See tests/mods/README.md for the command.
//   argv[1]  the root of the repository
#include "scripting/modhost.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace fs = std::filesystem;
using namespace Scripting;

#define CHECK(cond) \
  do { \
    if(!(cond)) { \
      std::fprintf(stderr,"%s:%d: check failed: %s\n",__FILE__,__LINE__,#cond); \
      dump(); \
      std::abort(); \
      } \
    } while(false)

namespace {

void dump() {
  for(auto& l:ModHost::inst().lines())
    std::fprintf(stderr,"  [%s] %s\n",l.source.c_str(),l.text.c_str());
  }

void write(const fs::path& file, const std::string& text) {
  fs::create_directories(file.parent_path());
  std::ofstream(file,std::ios::binary) << text;
  }

std::string eval(ModHost& h, const std::string& code) {
  bool ok = false;
  auto s  = h.evaluate(*h.find("devtest"),code,ok);
  if(!ok)
    std::fprintf(stderr,"eval failed: %s\n  %s\n",code.c_str(),s.c_str());
  CHECK(ok);
  return s;
  }

bool yes(ModHost& h, const std::string& code) {
  return eval(h,code)=="true";
  }

}

int main(int argc, char** argv) {
  const fs::path repo = argc>1 ? fs::path(argv[1]) : fs::current_path();
  const fs::path root = fs::temp_directory_path()/"og-devtools-test";
  std::error_code ec;
  fs::remove_all(root,ec);

  auto& host = ModHost::inst();
  host.setGame("G2");
  host.setEnabledByDefault(false);
  host.setDataDir(root/"data");
  host.addRoot(root/"mods");

  // the mod of the program, with the globals that the game gives to it
  const fs::path dir = root/"mods"/"devtest";
  write(dir/"manifest.json",R"({"id":"devtest"})");
  fs::create_directories(dir/"lib");
  fs::copy(repo/"mods"/"devtools"/"lib",dir/"lib",fs::copy_options::recursive|fs::copy_options::overwrite_existing);
  fs::copy_file(repo/"mods"/"devtools"/"index.js",dir/"real.js",fs::copy_options::overwrite_existing);
  fs::copy_file(repo/"tests"/"mods"/"fakeui.js",dir/"fakeui.js",fs::copy_options::overwrite_existing);
  fs::copy_file(repo/"tests"/"mods"/"fakedebug.js",dir/"fakedebug.js",fs::copy_options::overwrite_existing);
  write(dir/"index.js",R"(
    const { FakeUi } = require("./fakeui.js");
    const { FakeDebug, fakeGame } = require("./fakedebug.js");
    globalThis.ui = new FakeUi();
    globalThis.debug = new FakeDebug();
    const g = fakeGame();
    globalThis.player = g.player;
    globalThis.npcs = g.npcs;
    globalThis.world = g.world;
    globalThis.spawned = g.spawned;
    require("./real.js");
    globalThis.draw = () => { ui.clear(); ui.tabs[0].fn(); };
  )");
  host.scan();
  CHECK(host.start(*host.find("devtest")));

  // the tab is there
  CHECK(eval(host,"ui.tabs.length")=="1");
  CHECK(eval(host,"ui.tabs[0].title")=="Debug");

  // --- the log
  eval(host,"draw()");
  CHECK(yes(host,"ui.has('[Log]') && ui.has('[Scripts]') && ui.has('[World]') && ui.has('[Ikarus]')"));
  CHECK(yes(host,"ui.has('0 log lines')"));
  eval(host,"debug.say('hello line'); debug.say('mem_ trouble', 1); debug.say('quiet', 2); draw()");
  CHECK(yes(host,"ui.has('hello line') && ui.has('mem_ trouble')"));
  CHECK(yes(host,"!ui.has('quiet')"));                                 // debug lines are off
  CHECK(yes(host,"ui.has('2 of 3 lines')"));
  eval(host,"ui.typed['##logfilter'] = '-hello'; draw()");              // a word with - must not be there
  CHECK(yes(host,"!ui.has('hello line') && ui.has('mem_ trouble')"));
  eval(host,"ui.typed['##logfilter'] = 'MEM_'; draw()");
  CHECK(yes(host,"!ui.has('hello line') && ui.has('mem_ trouble')"));
  eval(host,"ui.typed['##logfilter'] = ''; draw()");
  CHECK(yes(host,"ui.has('hello line')"));
  CHECK(yes(host,"ui.scrolled > 0"));                                  // follows the end of the log
  eval(host,"ui.press('clear'); draw(); draw()");
  CHECK(yes(host,"!ui.has('hello line')"));

  // --- scripts
  eval(host,"ui.press('Scripts'); draw()");
  eval(host,"ui.typed['##fnquery'] = 'foo'; draw()");
  CHECK(yes(host,"ui.has('{FOO_ADD}') && !ui.has('{B_GIVEINVITEMS}')"));
  eval(host,"ui.press('FOO_ADD'); draw()");
  CHECK(yes(host,"ui.has('func int FOO_ADD(int a, string b)')"));
  eval(host,"ui.typed['a (int)'] = '41'; ui.typed['b (string)'] = 'x'; ui.press('Call'); draw()");
  CHECK(yes(host,"debug.calls.length == 1 && debug.calls[0].name == 'FOO_ADD'"));
  CHECK(yes(host,"debug.calls[0].args[0] === 41 && debug.calls[0].args[1] === 'x'"));
  CHECK(yes(host,"ui.has('returned 42') && ui.has('called FOO_ADD')"));   // the answer and the log of the call

  eval(host,"ui.typed['##fnquery'] = 'give'; draw(); ui.press('B_GIVEINVITEMS'); draw()");
  CHECK(yes(host,"ui.has('c_npc giver')"));
  eval(host,"ui.press('Call'); draw()");
  CHECK(yes(host,"ui.has('script error in B_GIVEINVITEMS')"));           // an error is shown, not thrown

  eval(host,"ui.typed['##symquery'] = 'foo'; draw(); ui.press('Foo'); draw()");
  CHECK(yes(host,"ui.has('Foo: string')"));
  eval(host,"ui.typed['value##symvalue'] = 'xyz'; ui.press('Set'); draw()");
  CHECK(eval(host,"debug.symbols_.Foo.value")=="xyz");

  // --- the world
  eval(host,"ui.press('World'); draw()");
  CHECK(yes(host,"ui.has('day 1, 08:05')"));
  eval(host,"ui.typed['##pointquery'] = 'oc'; draw(); ui.press('OC2'); draw()");
  CHECK(yes(host,"ui.has('{OC1}') && !ui.has('{PSI_START}')"));
  eval(host,"ui.press('Go to OC2'); draw()");
  CHECK(eval(host,"player.at")=="OC2");
  CHECK(yes(host,"ui.has('at OC2')"));

  eval(host,"ui.typed['##trigquery'] = 'evt'; draw()");
  CHECK(yes(host,"ui.has('EVT_GATE  [mover closed]') && ui.has('{EVT_TRAP}')"));
  eval(host,"ui.press('EVT_GATE  [mover closed]'); draw(); ui.press('Trigger'); draw(); ui.press('Untrigger'); draw()");
  CHECK(eval(host,"JSON.stringify(debug.triggered)")=="[[\"EVT_GATE\",false],[\"EVT_GATE\",true]]");

  eval(host,"ui.press('Set the time'); draw()");
  CHECK(eval(host,"world.t.hour")=="12");

  eval(host,"ui.typed['instance of a character##spawn'] = ' wolf '; ui.press('Spawn'); draw()");
  CHECK(eval(host,"spawned.length")=="1");
  CHECK(eval(host,"spawned[0].name")=="WOLF");
  CHECK(yes(host,"ui.has('WOLF is here')"));
  eval(host,"ui.typed['instance of a character##spawn'] = 'nobody'; ui.press('Spawn'); draw()");
  CHECK(yes(host,"ui.has('there is no instance nobody')"));

  eval(host,"ui.typed['instance of an item##item'] = 'itfo_apple'; ui.typed['count##count'] = '3'; ui.press('Give'); draw()");
  CHECK(eval(host,"player.items.ITFO_APPLE")=="3");

  CHECK(yes(host,"ui.has('Hero: 50/100 HP, level 3')"));
  eval(host,"ui.press('Heal'); draw()");
  CHECK(eval(host,"player.health")=="100");
  eval(host,"ui.typed['health##hp'] = '25'; ui.press('Set health'); draw()");
  CHECK(eval(host,"player.health")=="25");

  // --- Ikarus
  eval(host,"ui.press('Ikarus'); draw()");
  CHECK(yes(host,"ui.has('This game has Ikarus 1, LeGo 2') && ui.has('movers: 2')"));
  eval(host,"ui.typed['filter##ikfilter'] = 'region'; draw()");
  CHECK(yes(host,"ui.has('region 1') && !ui.has('movers: 2')"));

  // the log goes on while another section is open
  eval(host,"debug.say('late line'); draw(); ui.press('Log'); draw()");
  CHECK(yes(host,"ui.has('late line')"));

  host.stopAll();
  std::printf("PASS\n");
  fs::remove_all(root,ec);
  return 0;
  }
