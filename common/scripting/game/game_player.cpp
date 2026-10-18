#include "scripting/gameapi.h"

#include "gothic.h"
#include "world/objects/npc.h"

using namespace Scripting;

/// player: the character of the player. It is a character like others, see game_npc.cpp.
/// It stays valid when the world changes, valid is false only when there is no game.
void game::installPlayer(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue proto  = JS_GetPropertyStr(ctx,global,"__npcProto");
  if(JS_IsObject(proto)) {
    JSValue player = JS_NewObjectProto(ctx,proto);
    JS_DefinePropertyValueStr(ctx,player,"__id",JS_NewUint32(ctx,PlayerId),0);
    JS_SetPropertyStr(ctx,global,"player",player);
    }
  JS_FreeValue(ctx,proto);
  JS_FreeValue(ctx,global);
  }
