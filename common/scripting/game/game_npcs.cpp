#include "scripting/gameapi.h"

#include <algorithm>
#include <cmath>

#include "gothic.h"
#include "world/objects/npc.h"
#include "world/world.h"

using namespace Scripting;

namespace {

JSValue push(JSContext* ctx, JSValue arr, uint32_t& n, Npc& npc) {
  JSValue w = game::wrapNpc(ctx,npc);
  if(!JS_IsUndefined(w))
    JS_SetPropertyUint32(ctx,arr,n++,w);
  return arr;
  }

/// npcs.nearby(radius): characters around the player, the nearest first
JSValue nearby(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue arr   = JS_NewArray(ctx);
  auto*   world = Gothic::inst().world();
  auto*   pl    = Gothic::inst().player();
  if(world==nullptr || pl==nullptr)
    return arr;

  double radius = 2000;
  if(argc>0)
    JS_ToFloat64(ctx,&radius,argv[0]);

  const auto        pos = pl->position();
  std::vector<Npc*> found;
  world->detectNpc(pos,float(radius),[&](Npc& n) {
    if(&n!=pl)
      found.push_back(&n);
    });
  std::sort(found.begin(),found.end(),[&](Npc* a, Npc* b) {
    const auto da = a->position()-pos;
    const auto db = b->position()-pos;
    return da.quadLength()<db.quadLength();
    });

  uint32_t n = 0;
  for(auto* npc:found)
    push(ctx,arr,n,*npc);
  return arr;
  }

/// npcs.find(instance): the first character of an instance, like "BAU_904_BAUER"
JSValue find(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  if(world==nullptr || argc<1)
    return JS_NULL;

  const auto id = game::findInstance(*world,js::str(ctx,argv[0]));
  if(id==size_t(-1))
    return JS_NULL;
  if(auto* npc = world->findNpcByInstance(id)) {
    JSValue w = game::wrapNpc(ctx,*npc);
    return JS_IsUndefined(w) ? JS_NULL : w;
    }
  return JS_NULL;
  }

JSValue count(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  auto* world = Gothic::inst().world();
  return JS_NewUint32(ctx,world==nullptr ? 0 : world->npcCount());
  }

}

/// npcs: characters of the world
void game::installNpcs(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue npcs   = JS_NewObject(ctx);
  js::fn(ctx,npcs,"nearby",nearby,1);
  js::fn(ctx,npcs,"find",  find,1);
  js::fn(ctx,npcs,"count", count,0);
  JS_SetPropertyStr(ctx,global,"npcs",npcs);
  JS_FreeValue(ctx,global);
  }
