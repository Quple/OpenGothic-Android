#include "scripting/modjs.h"

using namespace Scripting;

Mod& js::mod(JSContext* ctx) {
  return *static_cast<Mod*>(JS_GetContextOpaque(ctx));
  }

std::string js::str(JSContext* ctx, JSValueConst v) {
  const char* s = JS_ToCString(ctx,v);
  if(s==nullptr) {
    JS_FreeValue(ctx,JS_GetException(ctx));
    return "<error>";
    }
  std::string ret = s;
  JS_FreeCString(ctx,s);
  return ret;
  }

std::string js::text(JSContext* ctx, JSValueConst v) {
  if(JS_IsString(v) || !JS_IsObject(v))
    return str(ctx,v);

  JSValue j = JS_JSONStringify(ctx,v,JS_UNDEFINED,JS_UNDEFINED);
  if(JS_IsException(j)) {
    JS_FreeValue(ctx,JS_GetException(ctx));
    return str(ctx,v);
    }
  std::string ret = JS_IsUndefined(j) ? str(ctx,v) : str(ctx,j);
  JS_FreeValue(ctx,j);
  return ret;
  }

std::string js::join(JSContext* ctx, int argc, JSValueConst* argv) {
  std::string ret;
  for(int i=0; i<argc; ++i) {
    if(i>0)
      ret += ' ';
    ret += text(ctx,argv[i]);
    }
  return ret;
  }

std::string js::exception(JSContext* ctx) {
  JSValue     ex  = JS_GetException(ctx);
  std::string ret = str(ctx,ex);
  if(JS_IsObject(ex)) {
    JSValue stack = JS_GetPropertyStr(ctx,ex,"stack");
    if(!JS_IsUndefined(stack)) {
      const auto s = str(ctx,stack);
      if(!s.empty())
        ret += "\n"+s;
      }
    JS_FreeValue(ctx,stack);
    }
  JS_FreeValue(ctx,ex);
  return ret;
  }

void js::fn(JSContext* ctx, JSValueConst obj, const char* name, JSCFunction* f, int argc) {
  JS_SetPropertyStr(ctx,obj,name,JS_NewCFunction(ctx,f,name,argc));
  }

void js::fnMagic(JSContext* ctx, JSValueConst obj, const char* name, Magic f, int argc, int magic) {
  JS_SetPropertyStr(ctx,obj,name,JS_NewCFunction2(ctx,reinterpret_cast<JSCFunction*>(f),name,argc,JS_CFUNC_generic_magic,magic));
  }
