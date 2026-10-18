#include "scripting/modjs.h"

#include <algorithm>

using namespace Scripting;

namespace {

JSValue set(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int repeat) {
  if(argc<1 || !JS_IsFunction(ctx,argv[0]))
    return JS_ThrowTypeError(ctx,"the first argument is a function");
  auto& m = js::mod(ctx);

  double ms = 0;
  if(argc>1)
    JS_ToFloat64(ctx,&ms,argv[1]);
  if(!(ms>=0))
    ms = 0;

  Mod::Timer t;
  t.id       = m.nextId++;
  t.interval = repeat ? std::max<uint64_t>(uint64_t(ms),1) : 0;
  t.due      = ModHost::inst().nowMs()+uint64_t(ms);
  t.fn       = JS_DupValue(ctx,argv[0]);
  for(int i=2; i<argc; ++i)
    t.args.push_back(JS_DupValue(ctx,argv[i]));
  m.timers.push_back(std::move(t));
  return JS_NewUint32(ctx,m.timers.back().id);
  }

JSValue clear(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  uint32_t id = 0;
  if(argc<1 || JS_ToUint32(ctx,&id,argv[0])!=0)
    return JS_UNDEFINED;

  auto& m = js::mod(ctx);
  for(size_t i=0; i<m.timers.size(); ++i) {
    if(m.timers[i].id!=id)
      continue;
    JS_FreeValue(ctx,m.timers[i].fn);
    for(auto& a:m.timers[i].args)
      JS_FreeValue(ctx,a);
    m.timers.erase(m.timers.begin()+std::ptrdiff_t(i));
    break;
    }
  return JS_UNDEFINED;
  }

}

/// setTimeout, setInterval, clearTimeout, clearInterval. The timers are driven by ModHost::tick.
void api::installTimers(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  js::fnMagic(ctx,global,"setTimeout", set,2,0);
  js::fnMagic(ctx,global,"setInterval",set,2,1);
  js::fn(ctx,global,"clearTimeout", clear,1);
  js::fn(ctx,global,"clearInterval",clear,1);
  JS_FreeValue(ctx,global);
  }
