#include "modhost.h"

#include <algorithm>
#include <cctype>

#include "modjs.h"
#include "modutil.h"

using namespace Scripting;

/// The lines of the console of the game: commands of the host and of the mods

namespace {

std::vector<std::string> words(std::string_view line) {
  std::vector<std::string> ret;
  std::string cur;
  bool        quoted = false;
  bool        any    = false;
  for(char c:line) {
    if(c=='"') {
      quoted = !quoted;
      any    = true;
      continue;
      }
    if(!quoted && std::isspace(static_cast<unsigned char>(c))) {
      if(any)
        ret.push_back(cur);
      cur.clear();
      any = false;
      continue;
      }
    cur.push_back(c);
    any = true;
    }
  if(any)
    ret.push_back(cur);
  return ret;
  }

std::string restOfLine(std::string_view line, std::string_view word) {
  const auto at = line.find(word);
  std::string rest(at==std::string_view::npos ? std::string_view() : line.substr(at+word.size()));
  while(!rest.empty() && std::isspace(static_cast<unsigned char>(rest.front())))
    rest.erase(rest.begin());
  return rest;
  }

}

std::string ModHost::evaluate(Mod& mod, std::string_view code, bool& ok) {
  ok = false;
  if(!mod.isRunning())
    return "the mod is not running";

  Mod* prev = running;
  running   = &mod;
  deadline  = steadyMs()+400;
  JSValue r = JS_Eval(mod.ctx,code.data(),code.size(),"<console>",JS_EVAL_TYPE_GLOBAL);
  deadline  = 0;
  running   = prev;

  std::string ret;
  if(JS_IsException(r)) {
    ret = js::exception(mod.ctx);
    } else {
    ret = JS_IsUndefined(r) ? "undefined" : js::text(mod.ctx,r);
    ok  = true;
    }
  JS_FreeValue(mod.ctx,r);
  pumpJobs();
  return ret;
  }

std::vector<std::string> ModHost::commandNames() const {
  std::vector<std::string> names = {"help","mods","start","stop","reset","enable","disable","eval","clear"};
  for(auto& m:list) {
    for(auto& c:m->commands)
      names.push_back(c.first);
    }
  std::sort(names.begin(),names.end());
  names.erase(std::unique(names.begin(),names.end()),names.end());
  return names;
  }

void ModHost::execute(std::string_view line) {
  while(!line.empty() && std::isspace(static_cast<unsigned char>(line.front())))
    line.remove_prefix(1);
  while(!line.empty() && std::isspace(static_cast<unsigned char>(line.back())))
    line.remove_suffix(1);
  if(line.empty())
    return;
  log(LogLine::Info,"console","> "+std::string(line));

  const auto args = words(line);
  if(args.empty())
    return;
  const auto cmd  = lowerCase(args[0]);

  auto modArg = [&](size_t at) -> Mod* {
    if(args.size()<=at) {
      log(LogLine::Warn,"console","the id of a mod is needed");
      return nullptr;
      }
    auto* m = find(args[at]);
    if(m==nullptr)
      log(LogLine::Warn,"console","there is no mod '"+args[at]+"'");
    return m;
    };

  if(cmd=="help") {
    std::string s = "commands:";
    for(auto& n:commandNames())
      s += " "+n;
    log(LogLine::Info,"console",s);
    for(auto& m:list) {
      for(auto& c:m->commands) {
        if(!c.second.help.empty() && m->isRunning())
          log(LogLine::Info,"console","  "+c.first+" - "+c.second.help+" ["+m->manifest.id+"]");
        }
      }
    return;
    }
  if(cmd=="clear") {
    clearLog();
    return;
    }
  if(cmd=="mods") {
    for(auto& m:list) {
      const char* st = m->state==Mod::State::Running ? "running" : m->state==Mod::State::Failed ? "failed" : "stopped";
      log(LogLine::Info,"console",m->manifest.id+" "+m->manifest.version+" "+st+(m->enabled ? " (enabled)" : ""));
      }
    if(list.empty())
      log(LogLine::Info,"console","no mods");
    return;
    }
  if(cmd=="start" || cmd=="stop" || cmd=="reset" || cmd=="enable" || cmd=="disable") {
    if(auto* m = modArg(1)) {
      if(cmd=="start")   start(*m); else
      if(cmd=="stop")    stop(*m); else
      if(cmd=="reset")   reset(*m); else
      if(cmd=="enable")  setEnabled(*m,true); else
                         setEnabled(*m,false);
      }
    return;
    }
  if(cmd=="eval") {
    if(auto* m = modArg(1)) {
      bool ok = false;
      const auto afterCommand = restOfLine(line,args[0]);
      const auto result       = evaluate(*m,restOfLine(afterCommand,args[1]),ok);
      log(ok ? LogLine::Info : LogLine::Error,m->manifest.id,result);
      }
    return;
    }

  // commands of the mods, "mod.command" chooses the mod
  std::string name = cmd;
  Mod*        only = nullptr;
  const auto  dot  = cmd.find('.');
  if(dot!=std::string::npos) {
    only = find(args[0].substr(0,dot));
    name = cmd.substr(dot+1);
    }

  bool called = false;
  for(auto& mp:list) {
    Mod& m = *mp;
    if(!m.isRunning() || (only!=nullptr && only!=&m))
      continue;
    auto it = m.commands.find(name);
    if(it==m.commands.end())
      continue;

    called = true;
    JSContext* ctx  = m.ctx;
    JSValue    arg  = JS_NewObject(ctx);
    JSValue    argv = JS_NewArray(ctx);
    for(size_t i=1; i<args.size(); ++i)
      JS_SetPropertyUint32(ctx,argv,uint32_t(i-1),JS_NewString(ctx,args[i].c_str()));
    JS_SetPropertyStr(ctx,arg,"args",argv);
    JS_SetPropertyStr(ctx,arg,"raw",JS_NewString(ctx,restOfLine(line,args[0]).c_str()));

    JSValue fn = JS_DupValue(ctx,it->second.fn);
    JSValue r  = invoke(m,fn,JS_UNDEFINED,1,&arg,"command '"+name+"'");
    JS_FreeValue(ctx,r);
    JS_FreeValue(ctx,fn);
    JS_FreeValue(ctx,arg);
    }
  if(!called)
    log(LogLine::Warn,"console","unknown command '"+args[0]+"', try help");
  }
