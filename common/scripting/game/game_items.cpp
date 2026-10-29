#include "scripting/gameapi.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "game/constants.h"
#include "gothic.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/world.h"

using namespace Scripting;

namespace {

/// The farthest an item can be to be taken by a mod: the player takes the item where it is (the animation is only
/// for show), so a mod cannot be allowed to pull the items from the other end of the world.
constexpr float MaxPickupDistance = 600.f;

void put(JSContext* ctx, JSValueConst obj, const char* name, double v) {
  JS_SetPropertyStr(ctx,obj,name,JS_NewFloat64(ctx,v));
  }

void put(JSContext* ctx, JSValueConst obj, const char* name, bool v) {
  JS_SetPropertyStr(ctx,obj,name,JS_NewBool(ctx,v));
  }

void put(JSContext* ctx, JSValueConst obj, const char* name, const std::string& v) {
  JS_SetPropertyStr(ctx,obj,name,JS_NewString(ctx,v.c_str()));
  }

/// items.nearby(radius): the items that lie on the ground around the player, the nearest first.
/// [{index, instance, instanceId, name, count, x, y, z, distance, gold, mission, owned, flags}, ...]
/// `index` is only good for items.pickup() in the same call of the script: it changes when an item is taken.
JSValue nearby(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue arr   = JS_NewArray(ctx);
  auto*   world = Gothic::inst().world();
  auto*   pl    = Gothic::inst().player();
  if(world==nullptr || pl==nullptr)
    return arr;

  double radius = 300;
  if(argc>0)
    JS_ToFloat64(ctx,&radius,argv[0]);
  radius = std::clamp(radius,0.0,double(MaxPickupDistance));

  struct Found {
    uint32_t index;
    Item*    item;
    float    dist2;
    };

  const auto         pos = pl->position();
  const float        r2  = float(radius*radius);
  std::vector<Found> found;
  for(uint32_t i=0; ; ++i) {
    Item* it = world->itmById(i);
    if(it==nullptr)
      break;
    const float d2 = (it->position()-pos).quadLength();
    if(d2<=r2)
      found.push_back({i,it,d2});
    }
  std::sort(found.begin(),found.end(),[](const Found& a, const Found& b) { return a.dist2<b.dist2; });

  uint32_t n = 0;
  for(auto& f:found) {
    Item& it = *f.item;
    JSValue o = JS_NewObject(ctx);
    const auto p = it.position();
    put(ctx,o,"index",     double(f.index));
    put(ctx,o,"instance",  game::instanceName(*world,it.clsId()));
    put(ctx,o,"instanceId",double(it.clsId()));
    put(ctx,o,"name",      std::string(it.displayName()));
    put(ctx,o,"count",     double(it.count()));
    put(ctx,o,"x",         double(p.x));
    put(ctx,o,"y",         double(p.y));
    put(ctx,o,"z",         double(p.z));
    put(ctx,o,"distance",  double(std::sqrt(f.dist2)));
    put(ctx,o,"gold",      it.isGold());
    put(ctx,o,"mission",   it.isMission());
    // an item of somebody: taking it is a theft, the characters around may see it
    put(ctx,o,"owned",     it.handle().owner!=0 || it.handle().owner_guild!=0);
    put(ctx,o,"flags",     double(uint32_t(it.itemFlag())));
    JS_SetPropertyUint32(ctx,arr,n++,o);
    }
  return arr;
  }

/// items.pickup(index): the player takes the item, the same way as with the action key (the animation, the theft
/// that the characters can see). true if the item is taken. It is not taken in a talk, in the pause, when the player
/// is down or busy, or when the item is farther than 6 m.
JSValue pickup(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  auto* pl    = Gothic::inst().player();
  uint32_t index = 0;
  if(world==nullptr || pl==nullptr || argc<1 || JS_ToUint32(ctx,&index,argv[0])!=0)
    return JS_FALSE;

  Item* it = world->itmById(index);
  if(it==nullptr)
    return JS_FALSE;
  if(Gothic::inst().isPause() || Gothic::inst().isInDialog() || pl->isDown())
    return JS_FALSE;
  if((it->position()-pl->position()).quadLength()>MaxPickupDistance*MaxPickupDistance)
    return JS_FALSE;
  return JS_NewBool(ctx,pl->takeItem(*it)!=nullptr);
  }

/// items.count(): how many items lie in the world
JSValue count(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  auto* world = Gothic::inst().world();
  uint32_t n = 0;
  if(world!=nullptr)
    while(world->itmById(n)!=nullptr)
      ++n;
  return JS_NewUint32(ctx,n);
  }

}

/// items: the items that lie in the world
void game::installItems(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue items  = JS_NewObject(ctx);
  js::fn(ctx,items,"nearby",nearby,1);
  js::fn(ctx,items,"pickup",pickup,1);
  js::fn(ctx,items,"count", count,0);
  JS_SetPropertyStr(ctx,global,"items",items);
  JS_FreeValue(ctx,global);
  }
