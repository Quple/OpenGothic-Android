#include "scripting/gameapi.h"

#include "gothic.h"

using namespace Scripting;

namespace {

/// screen.print(text): a message on the screen, like the messages of the game
JSValue print(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  Gothic::inst().onPrint(js::join(ctx,argc,argv));
  return JS_UNDEFINED;
  }

}

/// screen: what the player sees
void game::installScreen(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue screen = JS_NewObject(ctx);
  js::fn(ctx,screen,"print",print,1);
  JS_SetPropertyStr(ctx,global,"screen",screen);
  JS_FreeValue(ctx,global);
  }
