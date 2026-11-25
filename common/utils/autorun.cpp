#include "autorun.h"

#include <Tempest/Application>
#include <Tempest/Log>
#include <Tempest/Platform>
#include <Tempest/SystemApi>

#include <cctype>
#include <cstdio>
#include <fstream>

#include "game/compatibility/gaplog.h"
#include "scripting/modhost.h"
#include "utils/gamelog.h"
#include "world/world.h"
#include "commandline.h"
#include "gothic.h"

using namespace Tempest;

namespace {
std::string trim(std::string_view s) {
  size_t b = 0, e = s.size();
  while(b<e && std::isspace(uint8_t(s[b])))
    ++b;
  while(e>b && std::isspace(uint8_t(s[e-1])))
    --e;
  return std::string(s.substr(b,e-b));
  }

void appendJson(std::string& out, std::string_view s) {
  out += '"';
  for(char c:s) {
    const auto u = uint8_t(c);
    if(c=='"' || c=='\\') {
      out += '\\';
      out += c;
      }
    else if(c=='\n')
      out += "\\n";
    else if(c=='\r')
      out += "\\r";
    else if(c=='\t')
      out += "\\t";
    else if(u<0x20) {
      char buf[8];
      std::snprintf(buf,sizeof(buf),"\\u%04x",unsigned(u));
      out += buf;
      }
    else
      out += c;   // the log is UTF-8
    }
  out += '"';
  }
}

AutoRun& AutoRun::inst() {
  static AutoRun r;
  return r;
  }

AutoRun::AutoRun() {
  auto& cmd = CommandLine::inst();
  const int limit = cmd.autoExit();
  if(cmd.autoRun().empty() && limit<=0)
    return;
  enabled   = true;
  startTime = Application::tickCount();
  if(limit>0)
    limitMs = uint64_t(limit)*1000;
  std::string_view all = cmd.autoRun();
  while(!all.empty()) {
    const size_t sep = all.find(';');
    auto c = trim(all.substr(0,sep));
    if(!c.empty())
      commands.push_back(std::move(c));
    if(sep==std::string_view::npos)
      break;
    all.remove_prefix(sep+1);
    }
  logSerial = GameLog::serial();
  Log::i("autorun: ",commands.size()," command(s), the limit ",limitMs/1000," s");
  }

void AutoRun::tick(bool worldReady) {
  if(!enabled || finished)
    return;
  const uint64_t now = Application::tickCount();
  if(now-startTime>=limitMs) {
    Log::e("autorun: the limit of ",limitMs/1000," s is over");
    finish("timeout");
    return;
    }
  if(!worldReady)
    return;
  if(readyTime==0) {
    readyTime = now;
    auto& lines = Scripting::ModHost::inst().lines();
    modSerial = lines.empty() ? 0 : lines.back().serial;
    Log::i("autorun: the world is loaded after ",(now-startTime)/1000," s");
    }
  if(now<waitUntil)
    return;
  if(!waitText.empty()) {
    // waitfor: a line of the console of the mods that has the text
    for(auto& l:Scripting::ModHost::inst().lines()) {
      if(l.serial<=scanSerial)
        continue;
      scanSerial = l.serial;
      if(l.text.find(waitText)!=std::string::npos) {
        Log::i("autorun: seen \"",waitText,"\"");
        waitText.clear();
        break;
        }
      }
    if(!waitText.empty()) {
      if(now<waitTextUntil)
        return;
      Log::e("autorun: \"",waitText,"\" did not come");
      waitText.clear();
      }
    }
  if(next>=commands.size()) {
    finish("done");
    return;
    }

  const std::string c = commands[next++];
  done.push_back(c);
  Log::i("autorun> ",c);
  if(c=="quit") {
    finish("done");
    return;
    }
  if(c.rfind("wait",0)==0 && (c.size()==4 || std::isspace(uint8_t(c[4])))) {
    double sec = 1;
    try { sec = std::stod(c.substr(4)); } catch(...) { Log::e("autorun: wait: not a number: ",c); }
    waitUntil = now + uint64_t(std::max(0.0,sec)*1000.0);
    return;
    }
  if(c.rfind("waitfor ",0)==0) {
    // waitfor <text> [seconds]: the text may have spaces; a number at the end is the limit (default: the rest of the run)
    std::string arg = trim(c.substr(8));
    uint64_t    lim = limitMs;
    const size_t sp = arg.find_last_of(' ');
    if(sp!=std::string::npos) {
      try {
        size_t used = 0;
        const double sec = std::stod(arg.substr(sp+1),&used);
        if(used==arg.size()-sp-1) {
          lim = uint64_t(std::max(0.0,sec)*1000.0);
          arg = trim(arg.substr(0,sp));
          }
        }
      catch(...) {}
      }
    if(arg.size()>=2 && arg.front()=='"' && arg.back()=='"')
      arg = arg.substr(1,arg.size()-2);
    waitText      = arg;
    waitTextUntil = now + lim;
    // the lines since the command before: it may have printed the text already
    scanSerial    = cmdSerial;
    return;
    }
  if(c=="gaps") {
    Compatibility::GapLog::inst().flush();
    return;
    }
  {
    auto& lines = Scripting::ModHost::inst().lines();
    cmdSerial = lines.empty() ? 0 : lines.back().serial;
  }
  Scripting::ModHost::inst().execute(c);
  }

void AutoRun::finish(const char* result) {
  finished = true;
  writeReport(result);
  Log::i("autorun: ",result,", the report is autorun-report.json");
  Tempest::SystemApi::exit();
  }

void AutoRun::writeReport(const char* result) {
  auto& gaps = Compatibility::GapLog::inst();
  gaps.flush();

  std::string s = "{\"schema\":1,\"result\":";
  appendJson(s,result);
  s += ",\"seconds\":" + std::to_string((Application::tickCount()-startTime)/1000);
  s += ",\"worldLoaded\":" + std::string(readyTime!=0 ? "true" : "false");
  if(auto* w = Gothic::inst().world()) {
    s += ",\"world\":";
    appendJson(s,w->name());
    }
  s += ",\"commands\":[";
  for(size_t i=0; i<commands.size(); ++i) {
    if(i>0)
      s += ',';
    appendJson(s,commands[i]);
    }
  s += "],\"commandsRun\":" + std::to_string(done.size());

  // what the mods said (comcheck prints PASS/FAIL there)
  s += ",\"console\":[";
  bool first = true;
  for(auto& l:Scripting::ModHost::inst().lines()) {
    if(l.serial<=modSerial)
      continue;
    if(!first)
      s += ',';
    first = false;
    static const char* lv[] = {"info","warn","error","debug"};
    s += "{\"level\":\"" + std::string(lv[std::min<int>(l.level,3)]) + "\",\"source\":";
    appendJson(s,l.source);
    s += ",\"text\":";
    appendJson(s,l.text);
    s += '}';
    }
  s += ']';

  // the errors of the log since the start (the newest 300)
  std::vector<GameLog::Line> lines;
  GameLog::since(logSerial,100000,lines);
  std::vector<const GameLog::Line*> errors;
  for(auto& l:lines)
    if(l.level==GameLog::Error)
      errors.push_back(&l);
  const size_t from = errors.size()>300 ? errors.size()-300 : 0;
  s += ",\"logErrors\":" + std::to_string(errors.size()) + ",\"errors\":[";
  for(size_t i=from; i<errors.size(); ++i) {
    if(i>from)
      s += ',';
    appendJson(s,errors[i]->text);
    }
  s += "],\"gapsTotal\":" + std::to_string(gaps.total());
  s += ",\"gaps\":" + gaps.json() + "}\n";

  std::ofstream f("autorun-report.json",std::ios::binary|std::ios::trunc);
  f.write(s.data(),std::streamsize(s.size()));
  }
