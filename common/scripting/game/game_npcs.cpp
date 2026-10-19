#include "scripting/gameapi.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "game/constants.h"
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

/// npcs.byId(id): a character by its id, null if it is not in this world
JSValue byId(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  uint32_t id = 0;
  if(world==nullptr || argc<1 || JS_ToUint32(ctx,&id,argv[0])!=0)
    return JS_NULL;
  if(auto* npc = world->npcByUid(id)) {
    JSValue w = game::wrapNpc(ctx,*npc);
    return JS_IsUndefined(w) ? JS_NULL : w;
    }
  return JS_NULL;
  }

/// npcs.spawn(instance, x, y, z, rotation, options): a new character, null if there is no such instance.
/// options: {puppet: true} - the character is a puppet from the start, its script and AI never run
///          {id: n} - the number of the character, the one that it has in the game of the host
///          {player: true} - the character of a player of another game: the characters of the world notice it
JSValue spawn(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  if(world==nullptr || argc<4)
    return JS_NULL;

  const auto id = game::findInstance(*world,js::str(ctx,argv[0]));
  double x = 0, y = 0, z = 0, a = 0;
  if(id==size_t(-1) || JS_ToFloat64(ctx,&x,argv[1])<0 || JS_ToFloat64(ctx,&y,argv[2])<0 || JS_ToFloat64(ctx,&z,argv[3])<0)
    return JS_NULL;
  if(argc>4)
    JS_ToFloat64(ctx,&a,argv[4]);

  bool     puppet = false, player = false;
  uint32_t number = 0;
  if(argc>5 && JS_IsObject(argv[5])) {
    JSValue p = JS_GetPropertyStr(ctx,argv[5],"puppet");
    puppet = JS_ToBool(ctx,p)>0;
    JS_FreeValue(ctx,p);
    JSValue pl = JS_GetPropertyStr(ctx,argv[5],"player");
    player = JS_ToBool(ctx,pl)>0;
    JS_FreeValue(ctx,pl);
    JSValue n = JS_GetPropertyStr(ctx,argv[5],"id");
    if(JS_IsNumber(n))
      JS_ToUint32(ctx,&number,n);
    JS_FreeValue(ctx,n);
    }

  // a character of the number that is in the world already is not made twice
  if(number!=0 && world->npcByUid(number)!=nullptr)
    return JS_NULL;
  Npc* npc = world->addNpcRemote(id,Tempest::Vec3(float(x),float(y),float(z)),number,puppet);
  if(npc==nullptr)
    return JS_NULL;
  npc->setDirection(float(a));
  npc->setRemotePlayer(player);
  npc->updateTransform();
  JSValue w = game::wrapNpc(ctx,*npc);
  return JS_IsUndefined(w) ? JS_NULL : w;
  }

/// npcs.snapshot(radius, centers): the state of the characters that are near the centers (the player by default):
/// [id, instance, x, y, z, rotation, health, maxHealth, animation, weaponState, weaponItem, overlays, armor, unconscious, rangedItem].
/// The puppets and the player are not there.
JSValue snapshot(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue arr   = JS_NewArray(ctx);
  auto*   world = Gothic::inst().world();
  if(world==nullptr)
    return arr;

  double radius = 3000;
  if(argc>0)
    JS_ToFloat64(ctx,&radius,argv[0]);

  std::vector<Tempest::Vec3> centers;
  if(argc>1 && JS_IsArray(argv[1])) {
    JSValue len = JS_GetPropertyStr(ctx,argv[1],"length");
    uint32_t n  = 0;
    JS_ToUint32(ctx,&n,len);
    JS_FreeValue(ctx,len);
    for(uint32_t i=0; i<n && i<64; ++i) {
      JSValue c = JS_GetPropertyUint32(ctx,argv[1],i);
      double  v[3] = {};
      const char* keys[3] = {"x","y","z"};
      for(int k=0; k<3; ++k) {
        JSValue p = JS_GetPropertyStr(ctx,c,keys[k]);
        JS_ToFloat64(ctx,&v[k],p);
        JS_FreeValue(ctx,p);
        }
      JS_FreeValue(ctx,c);
      centers.emplace_back(float(v[0]),float(v[1]),float(v[2]));
      }
    }
  if(centers.empty()) {
    if(auto* pl = Gothic::inst().player())
      centers.push_back(pl->position());
    }

  std::set<uint32_t> seen;
  uint32_t           n = 0;
  for(auto& c:centers) {
    world->detectNpc(c,float(radius),[&](Npc& npc) {
      if(npc.isPlayer() || npc.isPuppet() || !seen.insert(npc.uid()).second)
        return;
      JSValue e = JS_NewArray(ctx);
      const auto p = npc.position();
      const double values[8] = {double(npc.uid()),double(npc.instanceSymbol()),double(p.x),double(p.y),double(p.z),
                                double(npc.rotation()),double(npc.attribute(ATR_HITPOINTS)),
                                double(npc.attribute(ATR_HITPOINTSMAX))};
      for(uint32_t i=0; i<8; ++i)
        JS_SetPropertyUint32(ctx,e,i,JS_NewFloat64(ctx,values[i]));
      // what it plays and what it holds
      JS_SetPropertyUint32(ctx,e,8,game::animationOf(ctx,npc));
      JS_SetPropertyUint32(ctx,e,9,JS_NewInt32(ctx,int32_t(npc.weaponState())));
      const size_t weapon = npc.meleeWeaponInstance();
      JS_SetPropertyUint32(ctx,e,10,JS_NewString(ctx,weapon==size_t(-1) ? "" : game::instanceName(*world,weapon).c_str()));
      JSValue overlays = JS_NewArray(ctx);
      uint32_t o = 0;
      for(auto& name:npc.overlayNames())
        JS_SetPropertyUint32(ctx,overlays,o++,JS_NewString(ctx,name.c_str()));
      JS_SetPropertyUint32(ctx,e,11,overlays);
      const size_t armor = npc.armorInstance();
      JS_SetPropertyUint32(ctx,e,12,JS_NewString(ctx,armor==size_t(-1) ? "" : game::instanceName(*world,armor).c_str()));
      JS_SetPropertyUint32(ctx,e,13,JS_NewInt32(ctx,npc.isUnconscious() ? 1 : 0));
      const size_t ranged = npc.rangedWeaponInstance();
      JS_SetPropertyUint32(ctx,e,14,JS_NewString(ctx,ranged==size_t(-1) ? "" : game::instanceName(*world,ranged).c_str()));
      JS_SetPropertyUint32(ctx,arr,n++,e);
      });
    }
  return arr;
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
  js::fn(ctx,npcs,"byId",  byId,1);
  js::fn(ctx,npcs,"spawn", spawn,5);
  js::fn(ctx,npcs,"snapshot",snapshot,2);
  JS_SetPropertyStr(ctx,global,"npcs",npcs);
  JS_FreeValue(ctx,global);
  }
