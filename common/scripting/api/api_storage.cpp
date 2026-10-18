#include "scripting/modjs.h"

using namespace Scripting;

namespace {

JSValue get(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<1)
    return JS_UNDEFINED;
  auto& m   = js::mod(ctx);
  auto  key = js::str(ctx,argv[0]);
  if(JS_IsObject(m.storage)) {
    JSValue v = JS_GetPropertyStr(ctx,m.storage,key.c_str());
    if(!JS_IsUndefined(v))
      return v;
    JS_FreeValue(ctx,v);
    }
  return argc>1 ? JS_DupValue(ctx,argv[1]) : JS_UNDEFINED;
  }

JSValue set(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<2)
    return JS_UNDEFINED;
  auto& m = js::mod(ctx);
  JS_SetPropertyStr(ctx,m.storage,js::str(ctx,argv[0]).c_str(),JS_DupValue(ctx,argv[1]));
  m.storageDirty = true;
  return JS_UNDEFINED;
  }

JSValue has(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<1)
    return JS_FALSE;
  auto&  m = js::mod(ctx);
  JSAtom a = JS_NewAtom(ctx,js::str(ctx,argv[0]).c_str());
  const int r = JS_HasProperty(ctx,m.storage,a);
  JS_FreeAtom(ctx,a);
  return JS_NewBool(ctx,r>0);
  }

JSValue remove(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<1)
    return JS_UNDEFINED;
  auto&  m = js::mod(ctx);
  JSAtom a = JS_NewAtom(ctx,js::str(ctx,argv[0]).c_str());
  JS_DeleteProperty(ctx,m.storage,a,0);
  JS_FreeAtom(ctx,a);
  m.storageDirty = true;
  return JS_UNDEFINED;
  }

JSValue keys(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  auto&   m = js::mod(ctx);
  JSValue r = JS_NewArray(ctx);
  JSPropertyEnum* tab = nullptr;
  uint32_t        len = 0;
  if(JS_GetOwnPropertyNames(ctx,&tab,&len,m.storage,JS_GPN_STRING_MASK|JS_GPN_ENUM_ONLY)==0) {
    for(uint32_t i=0; i<len; ++i)
      JS_SetPropertyUint32(ctx,r,i,JS_AtomToString(ctx,tab[i].atom));
    JS_FreePropertyEnum(ctx,tab,len);
    }
  return r;
  }

JSValue clear(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  auto& m = js::mod(ctx);
  JS_FreeValue(ctx,m.storage);
  m.storage      = JS_NewObject(ctx);
  m.storageDirty = true;
  return JS_UNDEFINED;
  }

JSValue save(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  ModHost::inst().saveStorage(js::mod(ctx));
  return JS_UNDEFINED;
  }

}

/// storage: values of a mod that stay between the games. The values are saved as JSON,
/// when the mod stops, every half of a minute and with storage.save().
void api::installStorage(JSContext* ctx, Mod& m) {
  JSValue global  = JS_GetGlobalObject(ctx);

  m.storage = JS_NewObject(ctx);
  JSValue storage = JS_NewObject(ctx);
  js::fn(ctx,storage,"get",   get,2);
  js::fn(ctx,storage,"set",   set,2);
  js::fn(ctx,storage,"has",   has,1);
  js::fn(ctx,storage,"remove",remove,1);
  js::fn(ctx,storage,"keys",  keys,0);
  js::fn(ctx,storage,"clear", clear,0);
  js::fn(ctx,storage,"save",  save,0);

  JS_SetPropertyStr(ctx,global,"storage",storage);
  JS_FreeValue(ctx,global);
  }
