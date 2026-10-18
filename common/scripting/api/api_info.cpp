#include "scripting/modjs.h"

using namespace Scripting;

/// GothicJS: the versions, the game and the mod itself
void api::installInfo(JSContext* ctx, Mod& m, ModHost& host) {
  JSValue global = JS_GetGlobalObject(ctx);

  JSValue info = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,info,"version", JS_NewString(ctx,HostVersion));
  JS_SetPropertyStr(ctx,info,"api",     JS_NewInt32(ctx,ApiVersion));
  JS_SetPropertyStr(ctx,info,"game",    JS_NewString(ctx,host.gameName().c_str()));
  JS_SetPropertyStr(ctx,info,"platform",JS_NewString(ctx,host.platformName().c_str()));

  JSValue self = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,self,"id",     JS_NewString(ctx,m.manifest.id.c_str()));
  JS_SetPropertyStr(ctx,self,"name",   JS_NewString(ctx,m.manifest.name.c_str()));
  JS_SetPropertyStr(ctx,self,"version",JS_NewString(ctx,m.manifest.version.c_str()));
  JS_SetPropertyStr(ctx,self,"author", JS_NewString(ctx,m.manifest.author.c_str()));
  JS_SetPropertyStr(ctx,info,"mod",self);

  JS_SetPropertyStr(ctx,global,"GothicJS",info);
  JS_FreeValue(ctx,global);
  }
