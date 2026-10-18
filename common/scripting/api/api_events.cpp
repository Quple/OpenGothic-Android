#include "scripting/modjs.h"

using namespace Scripting;

namespace {

JSValue on(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int once) {
  if(argc<2 || !JS_IsFunction(ctx,argv[1]))
    return JS_ThrowTypeError(ctx,"events.on(name, handler)");

  auto& m = js::mod(ctx);
  Mod::Handler h;
  h.id   = m.nextId++;
  h.fn   = JS_DupValue(ctx,argv[1]);
  h.once = once!=0;
  m.events[js::str(ctx,argv[0])].push_back(h);
  return JS_NewUint32(ctx,h.id);
  }

/// off(name) removes all handlers, off(name, handler) or off(name, id) one
JSValue off(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<1)
    return JS_UNDEFINED;
  auto& m  = js::mod(ctx);
  auto  it = m.events.find(js::str(ctx,argv[0]));
  if(it==m.events.end())
    return JS_UNDEFINED;

  uint32_t   id   = 0;
  const bool byId = argc>1 && JS_IsNumber(argv[1]) && JS_ToUint32(ctx,&id,argv[1])==0;
  auto&      hs   = it->second;
  for(size_t i=0; i<hs.size(); ) {
    const bool same = argc<2 ? true : byId ? hs[i].id==id : JS_IsStrictEqual(ctx,hs[i].fn,argv[1])!=0;
    if(!same) {
      ++i;
      continue;
      }
    JS_FreeValue(ctx,hs[i].fn);
    hs.erase(hs.begin()+std::ptrdiff_t(i));
    }
  return JS_UNDEFINED;
  }

/// The events of a mod are its own, nobody else hears them
JSValue emit(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<1)
    return JS_UNDEFINED;
  ModHost::inst().emitTo(js::mod(ctx),js::str(ctx,argv[0]),argc>1 ? argv[1] : JS_UNDEFINED,argc>1);
  return JS_UNDEFINED;
  }

}

/// events.on, once, off, emit
void api::installEvents(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue events = JS_NewObject(ctx);

  js::fnMagic(ctx,events,"on",  on,2,0);
  js::fnMagic(ctx,events,"once",on,2,1);
  js::fn(ctx,events,"off", off,2);
  js::fn(ctx,events,"emit",emit,2);

  JS_SetPropertyStr(ctx,global,"events",events);
  JS_FreeValue(ctx,global);
  }
