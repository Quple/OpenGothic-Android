#include "scripting/gameapi.h"

#include "game/gametime.h"
#include "gothic.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/waypoint.h"
#include "world/world.h"

using namespace Scripting;

namespace {

JSValue name(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  auto* world = Gothic::inst().world();
  return JS_NewString(ctx,world==nullptr ? "" : std::string(world->name()).c_str());
  }

/// world.time(): {day, hour, minute}
JSValue time(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  auto* world = Gothic::inst().world();
  if(world==nullptr)
    return JS_NULL;
  const auto t = world->time();
  JSValue o = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,o,"day",   JS_NewInt64(ctx,int64_t(t.day())));
  JS_SetPropertyStr(ctx,o,"hour",  JS_NewInt32(ctx,int32_t(t.hour())));
  JS_SetPropertyStr(ctx,o,"minute",JS_NewInt32(ctx,int32_t(t.minute())));
  return o;
  }

JSValue setTime(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  if(world==nullptr || argc<2)
    return JS_UNDEFINED;
  int32_t h = 0, m = 0;
  JS_ToInt32(ctx,&h,argv[0]);
  JS_ToInt32(ctx,&m,argv[1]);
  world->setDayTime(h,m);
  return JS_UNDEFINED;
  }

/// world.waypoint("OC2"): {x, y, z} or null
JSValue waypoint(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  if(world==nullptr || argc<1)
    return JS_NULL;
  auto* wp = world->findPoint(js::str(ctx,argv[0]),false);
  if(wp==nullptr)
    return JS_NULL;

  const auto pos = wp->position();
  JSValue o = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,o,"x",JS_NewFloat64(ctx,double(pos.x)));
  JS_SetPropertyStr(ctx,o,"y",JS_NewFloat64(ctx,double(pos.y)));
  JS_SetPropertyStr(ctx,o,"z",JS_NewFloat64(ctx,double(pos.z)));
  return o;
  }

/// world.takeShots(): the arrows and bolts that were shot since the last call (the first call starts to watch):
/// [[item, x, y, z, vx, vy, vz, shooter], ...]
JSValue takeShots(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  JSValue arr   = JS_NewArray(ctx);
  auto*   world = Gothic::inst().world();
  if(world==nullptr)
    return arr;
  uint32_t n = 0;
  for(auto& s:world->takeShots()) {
    JSValue e = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx,e,0,JS_NewString(ctx,game::instanceName(*world,s.item).c_str()));
    const double v[6] = {double(s.pos.x),double(s.pos.y),double(s.pos.z),double(s.vel.x),double(s.vel.y),double(s.vel.z)};
    for(uint32_t i=0; i<6; ++i)
      JS_SetPropertyUint32(ctx,e,i+1,JS_NewFloat64(ctx,v[i]));
    JS_SetPropertyUint32(ctx,e,7,JS_NewUint32(ctx,s.origin));
    JS_SetPropertyUint32(ctx,arr,n++,e);
    }
  return arr;
  }

/// world.shoot(item, x, y, z, vx, vy, vz): an arrow or a bolt that flies, as it was shot in another game. It makes
/// no damage here: the one who has shot it takes care of that. Spells are not shot this way.
JSValue shoot(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  if(world==nullptr || argc<7)
    return JS_FALSE;
  const auto cls = game::findInstance(*world,js::str(ctx,argv[0]));
  double v[6] = {};
  for(int i=0; i<6; ++i)
    if(JS_ToFloat64(ctx,&v[i],argv[i+1])<0)
      return JS_FALSE;
  if(cls==size_t(-1))
    return JS_FALSE;

  Item itm(*world,cls,Item::T_Inventory);
  const auto* b = world->shootRemote(itm,Tempest::Vec3(float(v[0]),float(v[1]),float(v[2])),
                                         Tempest::Vec3(float(v[3]),float(v[4]),float(v[5])));
  return JS_NewBool(ctx,b!=nullptr);
  }

}

/// world: the name of the world, the time, the way points
void game::installWorld(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue world  = JS_NewObject(ctx);
  js::fn(ctx,world,"name",    name,0);
  js::fn(ctx,world,"time",    time,0);
  js::fn(ctx,world,"setTime", setTime,2);
  js::fn(ctx,world,"waypoint",waypoint,1);
  js::fn(ctx,world,"takeShots",takeShots,0);
  js::fn(ctx,world,"shoot",   shoot,7);
  JS_SetPropertyStr(ctx,global,"world",world);
  JS_FreeValue(ctx,global);
  }
