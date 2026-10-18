#include "scripting/modjs.h"
#include "scripting/modutil.h"

using namespace Scripting;

namespace {

JSValue print(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int level) {
  auto& m = js::mod(ctx);
  ModHost::inst().log(ModHost::LogLine::Level(level),m.manifest.id,js::join(ctx,argc,argv));
  return JS_UNDEFINED;
  }

JSValue registerCommand(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<2 || !JS_IsFunction(ctx,argv[1]))
    return JS_ThrowTypeError(ctx,"registerCommand(name, handler, help)");

  auto& m    = js::mod(ctx);
  auto  name = lowerCase(js::str(ctx,argv[0]));
  if(name.empty() || name.find(' ')!=std::string::npos)
    return JS_ThrowTypeError(ctx,"the name of a command has no spaces");

  auto it = m.commands.find(name);
  if(it!=m.commands.end())
    JS_FreeValue(ctx,it->second.fn);

  Mod::Command c;
  c.fn   = JS_DupValue(ctx,argv[1]);
  c.help = argc>2 && !JS_IsUndefined(argv[2]) ? js::str(ctx,argv[2]) : std::string();
  m.commands[name] = std::move(c);
  return JS_UNDEFINED;
  }

JSValue unregisterCommand(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<1)
    return JS_UNDEFINED;
  auto& m  = js::mod(ctx);
  auto  it = m.commands.find(lowerCase(js::str(ctx,argv[0])));
  if(it!=m.commands.end()) {
    JS_FreeValue(ctx,it->second.fn);
    m.commands.erase(it);
    }
  return JS_UNDEFINED;
  }

}

/// console.log, warn, error, debug and the commands of the console of the game
void api::installConsole(JSContext* ctx) {
  JSValue global  = JS_GetGlobalObject(ctx);
  JSValue console = JS_NewObject(ctx);

  js::fnMagic(ctx,console,"log",  print,0,ModHost::LogLine::Info);
  js::fnMagic(ctx,console,"info", print,0,ModHost::LogLine::Info);
  js::fnMagic(ctx,console,"warn", print,0,ModHost::LogLine::Warn);
  js::fnMagic(ctx,console,"error",print,0,ModHost::LogLine::Error);
  js::fnMagic(ctx,console,"debug",print,0,ModHost::LogLine::Debug);
  js::fn(ctx,console,"registerCommand",  registerCommand,3);
  js::fn(ctx,console,"unregisterCommand",unregisterCommand,1);

  JS_SetPropertyStr(ctx,global,"console",console);
  JS_FreeValue(ctx,global);
  }
