#include "scripting/gameapi.h"

#include "game/gametime.h"
#include "gothic.h"
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

}

/// world: the name of the world, the time, the way points
void game::installWorld(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue world  = JS_NewObject(ctx);
  js::fn(ctx,world,"name",    name,0);
  js::fn(ctx,world,"time",    time,0);
  js::fn(ctx,world,"setTime", setTime,2);
  js::fn(ctx,world,"waypoint",waypoint,1);
  JS_SetPropertyStr(ctx,global,"world",world);
  JS_FreeValue(ctx,global);
  }
