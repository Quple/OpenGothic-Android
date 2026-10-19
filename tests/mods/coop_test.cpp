// Host-side test of the co-op mod (mods/coop): two games, a host and a client, in one process over the loopback.
// The game is pretended by tests/mods/fakegame.js. See tests/mods/README.md for the command.
//   argv[1]  the root of the repository
#include "scripting/modhost.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>

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

bool logged(ModHost& h, const std::string& text) {
  for(auto& l:h.lines()) {
    if(l.text.find(text)!=std::string::npos)
      return true;
    }
  return false;
  }

uint64_t clockMs = 1000;

/// Frames of the game: the network and the timers, then the event of every frame
void frames(ModHost& h, int n) {
  for(int i=0; i<n; ++i) {
    clockMs += 16;
    h.tick(clockMs);
    h.emit("tick",[](JSContext* ctx) {
      JSValue o = JS_NewObject(ctx);
      JS_SetPropertyStr(ctx,o,"dt",  JS_NewFloat64(ctx,16));
      JS_SetPropertyStr(ctx,o,"time",JS_NewFloat64(ctx,double(clockMs)));
      return o;
      });
    std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
  }

std::string eval(ModHost& h, const char* id, const std::string& code) {
  bool ok = false;
  auto s  = h.evaluate(*h.find(id),code,ok);
  if(!ok)
    std::fprintf(stderr,"eval failed: %s\n  %s\n",code.c_str(),s.c_str());
  CHECK(ok);
  return s;
  }

double num(ModHost& h, const char* id, const std::string& code) {
  return std::atof(eval(h,id,code).c_str());
  }

bool yes(ModHost& h, const char* id, const std::string& code) {
  return eval(h,id,code)=="true";
  }

/// A mod that is one of the games
std::string gameMod(const std::string& role, int port) {
  std::ostringstream s;
  s << "const ROLE = '" << role << "'; const PORT = " << port << ";\n" << R"(
    const { FakeGame } = require("./fakegame.js");
    const { createCoop } = require("./lib/coop.js");

    const game = new FakeGame("WORLD");
    game.add(1, 10, 100, 0, 100);
    game.add(2, 11, 200, 0, 200);
    game.add(3, 12, 300, 0, 300);
    game.add(4, 13, 90000, 0, 0);                      // too far from the players
    game.add(5, ROLE === "host" ? 14 : 99, 400, 0, 400);   // not the same character in the two games
    game.byUid.get(1).walk = { x: 50, z: 0 };
    if (ROLE === "client") { game.player.x = 5000; game.player.name = "Guest"; }

    let clock = 0;
    const env = {
      net, player: game.player, npcs: game.npcs,
      worldName: () => game.worldName || null,
      now: () => clock,
      takeShots: () => game.takeShots(),
      shoot: (...a) => game.shoot(...a),
      print: (t) => console.log("PRINT " + ROLE + " " + t),
      log: (l, t) => console.log("[" + l + "] " + ROLE + " " + t),
      avatarInstance: (i) => i,
    };
    const coop = createCoop(env);
    globalThis.game = game;
    globalThis.coop = coop;

    events.on("net:connect", (e) => coop.onConnect(e));
    events.on("net:message", (e) => coop.onMessage(e));
    events.on("net:disconnect", (e) => coop.onDisconnect(e));
    events.on("net:error", (e) => coop.onError(e));
    events.on("world:change", () => coop.onWorldChange());
    events.on("tick", ({ dt, time }) => { clock = time; game.step(dt); coop.tick(dt, time); });
  )";
  return s.str();
  }

void install(const fs::path& root, const fs::path& repo, const std::string& id, const std::string& role, int port) {
  const fs::path dir = root/"mods"/id;
  write(dir/"manifest.json","{\"id\":\""+id+"\"}");
  write(dir/"index.js",gameMod(role,port));
  fs::create_directories(dir/"lib");
  fs::copy(repo/"mods"/"coop"/"lib",dir/"lib",fs::copy_options::recursive|fs::copy_options::overwrite_existing);
  fs::copy_file(repo/"tests"/"mods"/"fakegame.js",dir/"fakegame.js",fs::copy_options::overwrite_existing);
  }

}

int main(int argc, char** argv) {
  const fs::path repo = argc>1 ? fs::path(argv[1]) : fs::current_path();
  const fs::path root = fs::temp_directory_path()/"og-coop-test";
  std::error_code ec;
  fs::remove_all(root,ec);

  auto& host = ModHost::inst();
  host.setGame("G1");
  host.setEnabledByDefault(false);
  host.setDataDir(root/"data");
  host.addRoot(root/"mods");

  install(root,repo,"hostgame","host",0);
  host.scan();
  CHECK(host.start(*host.find("hostgame")));

  // the host waits, the other game joins it
  const int port = int(num(host,"hostgame","coop.host(0)"));
  CHECK(port>0);
  CHECK(logged(host,"waiting for the players on port "+std::to_string(port)));
  CHECK(eval(host,"hostgame","coop.mode")=="host");

  install(root,repo,"clientgame","client",port);
  host.scan();
  CHECK(host.start(*host.find("clientgame")));
  eval(host,"clientgame","coop.join('127.0.0.1', PORT)");
  CHECK(eval(host,"clientgame","coop.mode")=="client");

  frames(host,150);
  CHECK(yes(host,"clientgame","coop.connected"));
  CHECK(num(host,"clientgame","coop.id")==1);
  CHECK(num(host,"hostgame","coop.players.size")==1);
  CHECK(logged(host,"Guest has joined"));
  CHECK(logged(host,"you are in the game, 1 other player(s)"));

  // the characters of the host are puppets in the other game
  CHECK(yes(host,"clientgame","game.byUid.get(1).puppet"));
  CHECK(yes(host,"clientgame","game.byUid.get(2).puppet"));
  CHECK(yes(host,"clientgame","game.byUid.get(3).puppet"));
  CHECK(!yes(host,"clientgame","game.byUid.get(4).puppet"));      // far from the players
  CHECK(!yes(host,"clientgame","game.byUid.get(5).puppet"));      // not the same character
  CHECK(num(host,"clientgame","coop.clientSync.mismatch")>=1);
  CHECK(!yes(host,"hostgame","game.byUid.get(1).puppet"));        // the host drives the characters

  // and they follow it
  frames(host,60);
  const double hx = num(host,"hostgame","game.byUid.get(1).x");
  const double cx = num(host,"clientgame","game.byUid.get(1).x");
  CHECK(hx>150);                                                   // it has walked
  CHECK(cx>150);
  CHECK(std::abs(hx-cx)<80);
  CHECK(num(host,"clientgame","game.byUid.get(1).moves")>5);

  // they play what the characters of the host play
  CHECK(eval(host,"clientgame","game.byUid.get(1).animName")=="S_RUNL");       // walks
  CHECK(eval(host,"clientgame","game.byUid.get(2).animName")=="S_IDLE");       // stands
  eval(host,"hostgame","game.byUid.get(2).playAnim('T_ATTACK'); game.byUid.get(2).ws = 2; game.byUid.get(2).wi = 'ITMW_SWORD';");
  frames(host,10);
  CHECK(eval(host,"clientgame","game.byUid.get(2).animName")=="T_ATTACK");
  CHECK(num(host,"clientgame","game.byUid.get(2).ws")==2);
  CHECK(eval(host,"clientgame","game.byUid.get(2).wi")=="ITMW_SWORD");
  // in the same phase of it
  frames(host,40);
  const double phaseHost   = num(host,"hostgame","game.time - game.byUid.get(2).animStart");
  const double phaseClient = num(host,"clientgame","game.time - game.byUid.get(2).animStart");
  CHECK(phaseHost>500);
  CHECK(std::abs(phaseHost-phaseClient)<150);
  // a standing character does not make the animation start again every message
  CHECK(num(host,"clientgame","game.byUid.get(3).animCalls")<=3);

  // characters that the host makes and removes are made and removed in the other games
  eval(host,"hostgame","game.add(20, 15, 600, 0, 600)");
  frames(host,60);
  CHECK(yes(host,"clientgame","!!game.byUid.get(20)"));
  CHECK(yes(host,"clientgame","game.byUid.get(20).puppet"));
  CHECK(eval(host,"clientgame","game.byUid.get(20).instance")=="INST_15");
  CHECK(num(host,"clientgame","game.byUid.get(20).instanceId")==15);
  CHECK(std::abs(num(host,"clientgame","game.byUid.get(20).x")-600)<5);
  CHECK(num(host,"clientgame","game.made.length")==1);
  eval(host,"hostgame","game.byUid.get(20).remove()");
  frames(host,40);
  CHECK(!yes(host,"clientgame","!!game.byUid.get(20)"));

  // the weapons that are carried are seen also when they are not drawn
  eval(host,"hostgame","game.byUid.get(2).ws = 0; game.byUid.get(2).wi = 'ITMW_SWORD_B'; game.byUid.get(2).rw = 'ITRW_BOW_L';");
  frames(host,20);
  CHECK(num(host,"clientgame","game.byUid.get(2).ws")==0);
  CHECK(eval(host,"clientgame","game.byUid.get(2).wi")=="ITMW_SWORD_B");
  CHECK(eval(host,"clientgame","game.byUid.get(2).rw")=="ITRW_BOW_L");

  // a guest talks to a character: the character is his own for a while, the host is not listened to
  eval(host,"clientgame","game.byUid.get(2).interact()");
  frames(host,5);
  CHECK(!yes(host,"clientgame","game.byUid.get(2).puppet"));
  eval(host,"hostgame","game.byUid.get(2).x = 900");
  eval(host,"clientgame","game.byUid.get(2).talking = true");
  frames(host,200);                                                // longer than the grace of a talk
  CHECK(!yes(host,"clientgame","game.byUid.get(2).puppet"));
  CHECK(std::abs(num(host,"clientgame","game.byUid.get(2).x")-900)>300);
  eval(host,"clientgame","game.byUid.get(2).talking = false");
  frames(host,400);                                                // the talk is over: the host decides again
  CHECK(yes(host,"clientgame","game.byUid.get(2).puppet"));
  CHECK(std::abs(num(host,"clientgame","game.byUid.get(2).x")-900)<5);

  // an unconscious character is unconscious in the other game too, and a guest can finish it off
  eval(host,"hostgame","game.add(6, 16, 700, 0, 700)");
  frames(host,60);
  CHECK(yes(host,"clientgame","game.byUid.get(6).puppet"));
  eval(host,"hostgame","game.byUid.get(6).unconscious = true; game.byUid.get(6).health = 1;");
  frames(host,30);
  CHECK(yes(host,"clientgame","game.byUid.get(6).unconscious"));
  CHECK(!yes(host,"clientgame","game.byUid.get(6).dead"));
  eval(host,"clientgame","game.byUid.get(6).finishRequest()");
  frames(host,30);
  CHECK(yes(host,"hostgame","game.byUid.get(6).dead"));
  CHECK(yes(host,"clientgame","game.byUid.get(6).dead"));

  // a character that is far from the players when it is removed is removed too
  eval(host,"hostgame","game.add(30, 17, 800, 0, 800)");
  frames(host,60);
  CHECK(yes(host,"clientgame","game.byUid.get(30).puppet"));
  eval(host,"hostgame","game.byUid.get(30).x = 80000");
  frames(host,40);
  CHECK(!yes(host,"clientgame","game.byUid.get(30).puppet"));      // far: it lives its own life here
  eval(host,"hostgame","game.byUid.get(30).remove()");
  frames(host,40);
  CHECK(!yes(host,"clientgame","!!game.byUid.get(30)"));

  // a character that the host has become (a player of the host controls it) is not there for the others
  eval(host,"hostgame","game.byUid.get(2).isPlayer = true");
  frames(host,40);
  CHECK(!yes(host,"clientgame","!!game.byUid.get(2)"));
  eval(host,"hostgame","game.byUid.get(2).isPlayer = false");
  frames(host,80);
  CHECK(yes(host,"clientgame","!!game.byUid.get(2)"));            // it is back: the guest makes it again
  CHECK(yes(host,"clientgame","game.byUid.get(2).puppet"));

  // the overlays and the armor
  eval(host,"hostgame","game.byUid.get(2).ov = ['HUMANS_RELAXED.MDS', 'HUMANS_1HST1.MDS']; game.byUid.get(2).ar = 'ITAR_MIL_L';");
  frames(host,20);
  CHECK(eval(host,"clientgame","game.byUid.get(2).overlays.join('+')")=="HUMANS_RELAXED.MDS+HUMANS_1HST1.MDS");
  CHECK(eval(host,"clientgame","game.byUid.get(2).armor")=="ITAR_MIL_L");

  // the players see each other
  CHECK(num(host,"clientgame","game.spawned.length")==1);        // the host
  CHECK(num(host,"hostgame","game.spawned.length")==1);          // the guest
  CHECK(yes(host,"clientgame","game.spawned[0].puppet"));
  CHECK(std::abs(num(host,"hostgame","game.spawned[0].x")-5000)<30);
  CHECK(std::abs(num(host,"clientgame","game.spawned[0].x")-0)<30);
  eval(host,"hostgame","game.player.x = 700; game.player.z = -300; game.player.rotation = 90;");
  frames(host,60);
  CHECK(std::abs(num(host,"clientgame","game.spawned[0].x")-700)<20);
  CHECK(std::abs(num(host,"clientgame","game.spawned[0].z")+300)<20);
  CHECK(std::abs(num(host,"clientgame","game.spawned[0].rotation")-90)<5);

  // the look of the players: the animation and the weapon
  eval(host,"hostgame","game.player.animName = 'S_WALKL'; game.player.animStart = game.time;");
  eval(host,"clientgame","game.player.animName = 'T_STRAFE'; game.player.animStart = game.time; game.player.weaponState = 2; game.player.weaponItem = 'ITMW_SWORD';");
  frames(host,60);
  CHECK(eval(host,"clientgame","game.spawned[0].animName")=="S_WALKL");
  CHECK(eval(host,"hostgame","game.spawned[0].animName")=="T_STRAFE");
  CHECK(num(host,"hostgame","game.spawned[0].ws")==2);
  CHECK(eval(host,"hostgame","game.spawned[0].wi")=="ITMW_SWORD");
  CHECK(num(host,"clientgame","game.spawned[0].ws")==0);

  eval(host,"clientgame","game.player.overlays = ['HUMANS_MILITIA.MDS']; game.player.armor = 'ITAR_SLD_M';");
  frames(host,30);
  CHECK(eval(host,"hostgame","game.spawned[0].overlays.join('+')")=="HUMANS_MILITIA.MDS");
  CHECK(eval(host,"hostgame","game.spawned[0].armor")=="ITAR_SLD_M");
  CHECK(yes(host,"hostgame","game.spawned[0].spawnedAsPuppet"));        // no script and no AI from the start
  CHECK(yes(host,"clientgame","game.spawned[0].spawnedAsPuppet"));

  // the host has become another character: his avatar looks like it
  eval(host,"hostgame","game.player.instance = 'PC_BULLDOG'");
  frames(host,40);
  CHECK(eval(host,"clientgame","game.spawned[game.spawned.length - 1].instance")=="PC_BULLDOG");
  CHECK(num(host,"clientgame","game.spawned.filter(n => n.valid).length")==1);
  eval(host,"hostgame","game.player.instance = 'PC_HERO'");
  frames(host,40);
  CHECK(eval(host,"clientgame","game.spawned[game.spawned.length - 1].instance")=="PC_HERO");
  CHECK(num(host,"clientgame","game.spawned.filter(n => n.valid).length")==1);

  // arrows fly in all the games
  eval(host,"hostgame","game.fire('ITRW_ARROW', 1, 2, 3, 100, 0, 0, 77)");
  frames(host,10);
  CHECK(eval(host,"clientgame","JSON.stringify(game.replayed)")=="[[\"ITRW_ARROW\",1,2,3,100,0,0]]");
  eval(host,"clientgame","game.fire('ITRW_BOLT', 5, 6, 7, 0, 0, 90, 0xFFFFFFFE); game.fire('ITRW_ARROW', 9, 9, 9, 1, 1, 1, 1234)");
  frames(host,10);
  CHECK(eval(host,"hostgame","JSON.stringify(game.replayed)")=="[[\"ITRW_BOLT\",5,6,7,0,0,90]]");   // only the shot of the player
  CHECK(eval(host,"clientgame","JSON.stringify(game.replayed)")=="[[\"ITRW_ARROW\",1,2,3,100,0,0]]");  // not its own again

  // the guest hits a character: the host takes the damage and everybody sees it
  eval(host,"clientgame","game.byUid.get(2).heal(-30)");
  frames(host,60);
  CHECK(num(host,"hostgame","game.byUid.get(2).health")==70);
  CHECK(num(host,"clientgame","game.byUid.get(2).health")==70);
  // the character knows who has hit it: the avatar of the guest, which the characters of the world notice
  CHECK(yes(host,"hostgame","game.byUid.get(2).lastAttacker === game.spawned.find(n => n.valid)"));
  CHECK(yes(host,"hostgame","game.spawned.find(n => n.valid).remotePlayer"));
  CHECK(yes(host,"clientgame","game.spawned.find(n => n.valid).remotePlayer"));

  // and the characters of the world hit the avatar: the guest takes the damage
  CHECK(num(host,"clientgame","game.player.health")==100);
  eval(host,"hostgame","game.spawned.find(n => n.valid).heal(-25)");
  frames(host,20);
  CHECK(num(host,"clientgame","game.player.health")==75);
  CHECK(num(host,"hostgame","game.player.health")==100);            // not the player of the host

  // a character that dies in the game of the host dies in the other too
  eval(host,"hostgame","game.byUid.get(3).setHealth(0)");
  frames(host,40);
  CHECK(yes(host,"clientgame","game.byUid.get(3).dead"));

  // chat
  eval(host,"clientgame","coop.chat('hello host')");
  frames(host,30);
  CHECK(logged(host,"PRINT host Guest: hello host"));

  // another world: the characters are not mixed
  eval(host,"clientgame","game.changeWorld('OTHER'); game.player.x = 1;");
  host.emitTo(*host.find("clientgame"),"world:change",JS_UNDEFINED,false);
  frames(host,60);
  CHECK(num(host,"clientgame","coop.clientSync.count")==0);
  CHECK(num(host,"hostgame","game.spawned.filter(n => n.valid).length")==0);   // the guest is not here

  // the guest comes back
  eval(host,"clientgame","game.changeWorld('WORLD'); game.add(1, 10, 100, 0, 100); game.add(2, 11, 200, 0, 200); game.add(3, 12, 300, 0, 300);");
  host.emitTo(*host.find("clientgame"),"world:change",JS_UNDEFINED,false);
  frames(host,80);
  CHECK(num(host,"hostgame","game.spawned.filter(n => n.valid).length")==1);
  CHECK(yes(host,"clientgame","game.byUid.get(1).puppet"));

  // the guest leaves
  eval(host,"clientgame","coop.leave()");
  frames(host,40);
  CHECK(logged(host,"Guest has left"));
  CHECK(num(host,"hostgame","coop.players.size")==0);
  CHECK(num(host,"hostgame","game.spawned.filter(n => n.valid).length")==0);

  // the host ends the game while a guest is in it: the guest gets its own characters back
  eval(host,"clientgame","coop.join('127.0.0.1', PORT)");
  frames(host,120);
  CHECK(yes(host,"clientgame","coop.connected"));
  CHECK(yes(host,"clientgame","game.byUid.get(1).puppet"));
  eval(host,"hostgame","coop.leave()");
  frames(host,40);
  CHECK(eval(host,"clientgame","coop.mode")=="off");
  CHECK(logged(host,"the connection with the host is lost"));

  // a bad message and a message of a stranger do not break anything
  eval(host,"hostgame","coop.host(0)");
  const int port2 = int(num(host,"hostgame","net.port"));
  write(root/"mods"/"intruder"/"manifest.json",R"({"id":"intruder"})");
  write(root/"mods"/"intruder"/"index.js","const p = net.connect('127.0.0.1', "+std::to_string(port2)+R"();
    events.on("net:connect", ({peer}) => {
      net.send(peer, "not json at all");
      net.send(peer, {t: "p", x: 1, y: 2, z: 3, a: 4});
      net.send(peer, {t: "hit", id: 1, d: 5});
      net.send(peer, {t: "hello", v: 99, name: "x", instance: "y"});
      net.send(peer, {t: "hello", v: 4, name: "Cheater", instance: "PC_HERO"});
      console.log("INTRUDER_SENT");
    });
  )");
  host.scan();
  CHECK(host.start(*host.find("intruder")));
  frames(host,60);
  CHECK(logged(host,"INTRUDER_SENT"));
  CHECK(logged(host,"a bad message from"));
  CHECK(logged(host,"Cheater has joined"));
  CHECK(num(host,"hostgame","coop.players.size")==1);   // only the one that has said hello right
  CHECK(num(host,"hostgame","game.byUid.get(1).health")==100);   // and the hit of a stranger did nothing

  host.stopAll();
  std::printf("PASS\n");
  fs::remove_all(root,ec);
  return 0;
  }
