#include "scripting/modjs.h"
#include "scripting/modutil.h"

using namespace Scripting;

namespace {

JSValue requireFrom(JSContext* ctx, const std::string& baseDir, const std::string& spec) {
  auto& m = js::mod(ctx);

  const bool relative = spec.rfind("./",0)==0 || spec.rfind("../",0)==0;
  std::string path;
  if(!normalizePath(relative && !baseDir.empty() ? baseDir+"/"+spec : spec,path))
    return JS_ThrowReferenceError(ctx,"'%s' is outside of the mod",spec.c_str());

  std::string found;
  for(const auto& candidate:{path,path+".js",path+".json",path+"/index.js"}) {
    if(m.source->exists(candidate)) {
      found = candidate;
      break;
      }
    }
  if(found.empty())
    return JS_ThrowReferenceError(ctx,"cannot find module '%s'",spec.c_str());

  // a file is run once
  auto cached = m.modules.find(found);
  if(cached!=m.modules.end())
    return JS_GetPropertyStr(ctx,cached->second,"exports");

  std::string code;
  if(!m.source->read(found,code))
    return JS_ThrowReferenceError(ctx,"cannot read '%s'",found.c_str());

  if(found.size()>5 && found.compare(found.size()-5,5,".json")==0)
    return JS_ParseJSON(ctx,code.data(),code.size(),found.c_str());

  JSValue module  = JS_NewObject(ctx);
  JSValue exports = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,module,"exports",JS_DupValue(ctx,exports));
  m.modules[found] = JS_DupValue(ctx,module);

  // the file is a body of a function
  const std::string wrapped = "(function(exports, require, module, __filename, __dirname){"+code+"\n})";
  JSValue f = JS_Eval(ctx,wrapped.data(),wrapped.size(),found.c_str(),JS_EVAL_TYPE_GLOBAL);
  if(JS_IsException(f)) {
    JS_FreeValue(ctx,module);
    JS_FreeValue(ctx,exports);
    return f;
    }

  const std::string dir = dirName(found);
  JSValue args[5] = {
    JS_DupValue(ctx,exports),
    api::makeRequire(ctx,dir),
    JS_DupValue(ctx,module),
    JS_NewString(ctx,found.c_str()),
    JS_NewString(ctx,dir.c_str())
    };
  JSValue r = JS_Call(ctx,f,JS_UNDEFINED,5,args);
  for(auto& a:args)
    JS_FreeValue(ctx,a);
  JS_FreeValue(ctx,f);
  JS_FreeValue(ctx,exports);

  if(JS_IsException(r)) {
    JS_FreeValue(ctx,module);
    auto it = m.modules.find(found);
    if(it!=m.modules.end()) {
      JS_FreeValue(ctx,it->second);
      m.modules.erase(it);
      }
    return r;
    }
  JS_FreeValue(ctx,r);

  JSValue ret = JS_GetPropertyStr(ctx,module,"exports");
  JS_FreeValue(ctx,module);
  return ret;
  }

JSValue require(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int, JSValueConst* data) {
  if(argc<1)
    return JS_ThrowTypeError(ctx,"require(path)");
  return requireFrom(ctx,js::str(ctx,data[0]),js::str(ctx,argv[0]));
  }

}

JSValue api::makeRequire(JSContext* ctx, const std::string& dir) {
  JSValue d = JS_NewString(ctx,dir.c_str());
  JSValue f = JS_NewCFunctionData(ctx,require,1,0,1,&d);
  JS_FreeValue(ctx,d);
  return f;
  }

/// require("./file.js") loads files of the mod, paths cannot leave the mod
void api::installRequire(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx,global,"require",makeRequire(ctx,""));
  JS_FreeValue(ctx,global);
  }
