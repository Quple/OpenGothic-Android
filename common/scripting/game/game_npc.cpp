#include "scripting/gameapi.h"

#include "game/constants.h"
#include "game/gamescript.h"
#include "gothic.h"
#include "world/objects/npc.h"
#include "world/world.h"

using namespace Scripting;

/// A character as the scripts see it: name, health, position, items. The object keeps only the number of the character,
/// so it is safe to keep it for as long as the mod wants.

namespace {

enum Prop : int {
  Valid, Id, Name, Instance, Level, Health, MaxHealth, Mana, MaxMana, Strength, Dexterity, Experience, Dead, GuildOf,
  IsPlayer, X, Y, Z, Position
  };

constexpr const char* HiddenId    = "__id";
constexpr const char* HiddenProto = "__npcProto";

Npc* self(JSContext* ctx, JSValueConst obj) {
  return game::resolveNpc(ctx,obj);
  }

JSValue get(JSContext* ctx, JSValueConst obj, int prop) {
  Npc* npc = self(ctx,obj);
  if(prop==Valid)
    return JS_NewBool(ctx,npc!=nullptr);
  if(prop==Id) {
    JSValue v = JS_GetPropertyStr(ctx,obj,HiddenId);
    return v;
    }
  if(npc==nullptr)
    return JS_UNDEFINED;

  switch(prop) {
    case Name:       return JS_NewString(ctx,std::string(npc->displayName()).c_str());
    case Instance:   return JS_NewString(ctx,game::instanceName(npc->world(),npc->instanceSymbol()).c_str());
    case Level:      return JS_NewInt32(ctx,npc->level());
    case Health:     return JS_NewInt32(ctx,npc->attribute(ATR_HITPOINTS));
    case MaxHealth:  return JS_NewInt32(ctx,npc->attribute(ATR_HITPOINTSMAX));
    case Mana:       return JS_NewInt32(ctx,npc->attribute(ATR_MANA));
    case MaxMana:    return JS_NewInt32(ctx,npc->attribute(ATR_MANAMAX));
    case Strength:   return JS_NewInt32(ctx,npc->attribute(ATR_STRENGTH));
    case Dexterity:  return JS_NewInt32(ctx,npc->attribute(ATR_DEXTERITY));
    case Experience: return JS_NewInt32(ctx,npc->experience());
    case Dead:       return JS_NewBool(ctx,npc->isDead());
    case GuildOf:      return JS_NewUint32(ctx,npc->guild());
    case IsPlayer:   return JS_NewBool(ctx,npc->isPlayer());
    case X:          return JS_NewFloat64(ctx,double(npc->position().x));
    case Y:          return JS_NewFloat64(ctx,double(npc->position().y));
    case Z:          return JS_NewFloat64(ctx,double(npc->position().z));
    case Position: {
      JSValue p = JS_NewObject(ctx);
      const auto pos = npc->position();
      JS_SetPropertyStr(ctx,p,"x",JS_NewFloat64(ctx,double(pos.x)));
      JS_SetPropertyStr(ctx,p,"y",JS_NewFloat64(ctx,double(pos.y)));
      JS_SetPropertyStr(ctx,p,"z",JS_NewFloat64(ctx,double(pos.z)));
      return p;
      }
    default: break;
    }
  return JS_UNDEFINED;
  }

JSValue number(JSContext* ctx, JSValueConst v, double& out) {
  if(JS_ToFloat64(ctx,&out,v)<0)
    return JS_EXCEPTION;
  return JS_UNDEFINED;
  }

// --- health, mana
JSValue setHealth(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  double v = 0;
  if(npc==nullptr || argc<1 || JS_IsException(number(ctx,argv[0],v)))
    return JS_UNDEFINED;
  const int32_t max   = npc->attribute(ATR_HITPOINTSMAX);
  const int32_t value = std::max<int32_t>(0,std::min<int32_t>(max,int32_t(v)));
  npc->changeAttribute(ATR_HITPOINTS,value-npc->attribute(ATR_HITPOINTS),true);
  return JS_UNDEFINED;
  }

/// heal(n) adds, heal(-n) hurts
JSValue heal(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  double v = 0;
  if(npc==nullptr || argc<1 || JS_IsException(number(ctx,argv[0],v)))
    return JS_UNDEFINED;
  npc->changeAttribute(ATR_HITPOINTS,int32_t(v),true);
  return JS_UNDEFINED;
  }

JSValue setMana(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  double v = 0;
  if(npc==nullptr || argc<1 || JS_IsException(number(ctx,argv[0],v)))
    return JS_UNDEFINED;
  const int32_t max   = npc->attribute(ATR_MANAMAX);
  const int32_t value = std::max<int32_t>(0,std::min<int32_t>(max,int32_t(v)));
  npc->changeAttribute(ATR_MANA,value-npc->attribute(ATR_MANA),true);
  return JS_UNDEFINED;
  }

JSValue kill(JSContext* ctx, JSValueConst obj, int, JSValueConst*) {
  Npc* npc = self(ctx,obj);
  if(npc!=nullptr && !npc->isDead())
    npc->changeAttribute(ATR_HITPOINTS,-npc->attribute(ATR_HITPOINTS),true);
  return JS_UNDEFINED;
  }

// --- place
JSValue setPosition(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<3)
    return JS_FALSE;
  double x = 0, y = 0, z = 0;
  if(JS_IsException(number(ctx,argv[0],x)) || JS_IsException(number(ctx,argv[1],y)) || JS_IsException(number(ctx,argv[2],z)))
    return JS_FALSE;
  return JS_NewBool(ctx,npc->setPosition(float(x),float(y),float(z)));
  }

JSValue teleport(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<1)
    return JS_FALSE;
  const auto name = js::str(ctx,argv[0]);
  auto*      wp   = npc->world().findPoint(name,false);
  if(wp==nullptr)
    return JS_FALSE;
  return JS_NewBool(ctx,npc->setPosition(wp->position()));
  }

// --- items
JSValue addItem(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<1)
    return JS_FALSE;

  const auto name = js::str(ctx,argv[0]);
  const auto id   = game::findInstance(npc->world(),name);
  double     n    = 1;
  if(argc>1)
    JS_ToFloat64(ctx,&n,argv[1]);
  if(id==size_t(-1) || n<1)
    return JS_FALSE;
  npc->addItem(id,size_t(n));
  return JS_TRUE;
  }

JSValue removeItem(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<1)
    return JS_FALSE;

  const auto id = game::findInstance(npc->world(),js::str(ctx,argv[0]));
  double     n  = 1;
  if(argc>1)
    JS_ToFloat64(ctx,&n,argv[1]);
  if(id==size_t(-1) || n<1)
    return JS_FALSE;
  const size_t have = npc->inventory().itemCount(id);
  npc->delItem(id,uint32_t(std::min(have,size_t(n))));
  return JS_TRUE;
  }

JSValue itemCount(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<1)
    return JS_NewInt32(ctx,0);
  const auto id = game::findInstance(npc->world(),js::str(ctx,argv[0]));
  if(id==size_t(-1))
    return JS_NewInt32(ctx,0);
  return JS_NewInt64(ctx,int64_t(npc->inventory().itemCount(id)));
  }

JSValue hasItem(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  JSValue n = itemCount(ctx,obj,argc,argv);
  int32_t c = 0;
  JS_ToInt32(ctx,&c,n);
  JS_FreeValue(ctx,n);
  return JS_NewBool(ctx,c>0);
  }

void getter(JSContext* ctx, JSValueConst proto, const char* name, Prop prop) {
  JSAtom a = JS_NewAtom(ctx,name);
  JSValue g = JS_NewCFunction2(ctx,reinterpret_cast<JSCFunction*>(&get),name,0,JS_CFUNC_getter_magic,prop);
  JS_DefinePropertyGetSet(ctx,proto,a,g,JS_UNDEFINED,JS_PROP_C_W_E);
  JS_FreeAtom(ctx,a);
  }

}

Npc* game::resolveNpc(JSContext* ctx, JSValueConst obj) {
  JSValue v  = JS_GetPropertyStr(ctx,obj,HiddenId);
  uint32_t id = 0;
  const bool ok = JS_IsNumber(v) && JS_ToUint32(ctx,&id,v)==0;
  JS_FreeValue(ctx,v);
  if(!ok)
    return nullptr;

  if(id==PlayerId)
    return Gothic::inst().player();
  auto* world = Gothic::inst().world();
  return world!=nullptr ? world->npcById(id) : nullptr;
  }

JSValue game::wrapNpc(JSContext* ctx, Npc& npc) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue proto  = JS_GetPropertyStr(ctx,global,HiddenProto);
  JS_FreeValue(ctx,global);
  if(!JS_IsObject(proto)) {
    JS_FreeValue(ctx,proto);
    return JS_UNDEFINED;
    }

  uint32_t id = PlayerId;
  if(!npc.isPlayer()) {
    const auto at = game::npcIndex(npc.world(),npc);
    if(at<0) {
      JS_FreeValue(ctx,proto);
      return JS_UNDEFINED;
      }
    id = uint32_t(at);
    }

  JSValue obj = JS_NewObjectProto(ctx,proto);
  JS_FreeValue(ctx,proto);
  JS_DefinePropertyValueStr(ctx,obj,HiddenId,JS_NewUint32(ctx,id),0);
  return obj;
  }

/// The prototype that every character has. Put in the global object, but hidden from the scripts.
void game::installNpc(JSContext* ctx) {
  JSValue proto = JS_NewObject(ctx);

  getter(ctx,proto,"valid",     Valid);
  getter(ctx,proto,"id",        Id);
  getter(ctx,proto,"name",      Name);
  getter(ctx,proto,"instance",  Instance);
  getter(ctx,proto,"level",     Level);
  getter(ctx,proto,"health",    Health);
  getter(ctx,proto,"maxHealth", MaxHealth);
  getter(ctx,proto,"mana",      Mana);
  getter(ctx,proto,"maxMana",   MaxMana);
  getter(ctx,proto,"strength",  Strength);
  getter(ctx,proto,"dexterity", Dexterity);
  getter(ctx,proto,"experience",Experience);
  getter(ctx,proto,"dead",      Dead);
  getter(ctx,proto,"guild",     GuildOf);
  getter(ctx,proto,"isPlayer",  IsPlayer);
  getter(ctx,proto,"x",         X);
  getter(ctx,proto,"y",         Y);
  getter(ctx,proto,"z",         Z);
  getter(ctx,proto,"position",  Position);

  js::fn(ctx,proto,"setHealth", setHealth,1);
  js::fn(ctx,proto,"heal",      heal,1);
  js::fn(ctx,proto,"setMana",   setMana,1);
  js::fn(ctx,proto,"kill",      kill,0);
  js::fn(ctx,proto,"setPosition",setPosition,3);
  js::fn(ctx,proto,"teleport",  teleport,1);
  js::fn(ctx,proto,"addItem",   addItem,2);
  js::fn(ctx,proto,"removeItem",removeItem,2);
  js::fn(ctx,proto,"itemCount", itemCount,1);
  js::fn(ctx,proto,"hasItem",   hasItem,1);

  JSValue global = JS_GetGlobalObject(ctx);
  JS_DefinePropertyValueStr(ctx,global,HiddenProto,proto,0);
  JS_FreeValue(ctx,global);
  }
