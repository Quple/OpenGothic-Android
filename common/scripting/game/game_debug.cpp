#include "scripting/gameapi.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>

#include "camera.h"
#include "game/gamescript.h"
#include "gothic.h"
#include "utils/gamelog.h"
#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/triggers/movetrigger.h"
#include "world/waypoint.h"
#include "world/world.h"

using namespace Scripting;

/// debug: for the developers of mods and of the game: the log of the game, the symbols and the functions of the
/// scripts of the game (Daedalus), the triggers and the points of the world. The menu mods/devtools is made of it.
namespace {

World* world() {
  return Gothic::inst().world();
  }

bool contains(std::string_view text, std::string_view filter) {
  if(filter.empty())
    return true;
  auto it = std::search(text.begin(),text.end(),filter.begin(),filter.end(),[](char a, char b) {
    return std::toupper(static_cast<unsigned char>(a))==std::toupper(static_cast<unsigned char>(b));
    });
  return it!=text.end();
  }

uint32_t number(JSContext* ctx, int argc, JSValueConst* argv, int at, uint32_t def) {
  uint32_t v = def;
  if(at<argc && !JS_IsUndefined(argv[at]) && JS_ToUint32(ctx,&v,argv[at])<0) {
    JS_FreeValue(ctx,JS_GetException(ctx));
    return def;
    }
  return v;
  }

const char* typeName(zenkit::DaedalusDataType t) {
  switch(t) {
    case zenkit::DaedalusDataType::VOID:      return "void";
    case zenkit::DaedalusDataType::FLOAT:     return "float";
    case zenkit::DaedalusDataType::INT:       return "int";
    case zenkit::DaedalusDataType::STRING:    return "string";
    case zenkit::DaedalusDataType::CLASS:     return "class";
    case zenkit::DaedalusDataType::FUNCTION:  return "func";
    case zenkit::DaedalusDataType::PROTOTYPE: return "prototype";
    case zenkit::DaedalusDataType::INSTANCE:  return "instance";
    default: break;
    }
  return "?";
  }

// --- log

/// debug.log(after, max) -> {serial, lines: [[level, text], ...]}: the lines after the serial; level 0 is info, 1 error, 2 debug
JSValue log(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  const uint64_t after = number(ctx,argc,argv,0,0);
  const size_t   max   = std::min<size_t>(number(ctx,argc,argv,1,400),2000);

  std::vector<GameLog::Line> lines;
  const uint64_t last = GameLog::since(after,max,lines);

  JSValue arr = JS_NewArray(ctx);
  for(uint32_t i=0; i<lines.size(); ++i) {
    JSValue e = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx,e,0,JS_NewInt32(ctx,int32_t(lines[i].level)));
    JS_SetPropertyUint32(ctx,e,1,JS_NewString(ctx,lines[i].text.c_str()));
    JS_SetPropertyUint32(ctx,arr,i,e);
    }
  JSValue ret = JS_NewObject(ctx);
  // when there are more lines than the answer holds, the next call goes on after the last one that was given
  const uint64_t serial = lines.empty() ? std::max<uint64_t>(after,last) : lines.back().serial;
  JS_SetPropertyStr(ctx,ret,"serial",JS_NewFloat64(ctx,double(serial)));
  JS_SetPropertyStr(ctx,ret,"lines",arr);
  return ret;
  }

// --- scripts of the game

/// debug.functions(filter, max) -> names of the functions of the scripts that have the text in the name
JSValue functions(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue arr = JS_NewArray(ctx);
  auto*   w   = world();
  if(w==nullptr)
    return arr;

  const std::string filter = argc>0 ? js::str(ctx,argv[0]) : std::string();
  const size_t      max    = std::min<size_t>(number(ctx,argc,argv,1,100),1000);
  auto&             vm     = w->script().getVm();
  uint32_t          n      = 0;
  for(auto& s:vm.symbols()) {
    if(s.type()!=zenkit::DaedalusDataType::FUNCTION || s.is_member() || !contains(s.name(),filter))
      continue;
    JS_SetPropertyUint32(ctx,arr,n++,JS_NewString(ctx,s.name().c_str()));
    if(n>=max)
      break;
    }
  return arr;
  }

/// debug.symbols(filter, max) -> names of the variables and the constants
JSValue symbols(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue arr = JS_NewArray(ctx);
  auto*   w   = world();
  if(w==nullptr)
    return arr;

  const std::string filter = argc>0 ? js::str(ctx,argv[0]) : std::string();
  const size_t      max    = std::min<size_t>(number(ctx,argc,argv,1,100),1000);
  auto&             vm     = w->script().getVm();
  uint32_t          n      = 0;
  for(auto& s:vm.symbols()) {
    const auto t = s.type();
    if(s.is_member() || (t!=zenkit::DaedalusDataType::INT && t!=zenkit::DaedalusDataType::FLOAT && t!=zenkit::DaedalusDataType::STRING))
      continue;
    if(s.name().find('.')!=std::string::npos || !contains(s.name(),filter))
      continue;
    JS_SetPropertyUint32(ctx,arr,n++,JS_NewString(ctx,s.name().c_str()));
    if(n>=max)
      break;
    }
  return arr;
  }

/// debug.signature(name) -> "func int NAME(int a, string b)" or null
JSValue signature(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* w = world();
  if(w==nullptr || argc<1)
    return JS_NULL;
  auto& vm  = w->script().getVm();
  auto* sym = vm.find_symbol_by_name(js::str(ctx,argv[0]));
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::FUNCTION)
    return JS_NULL;

  std::string text = std::string("func ")+(sym->has_return() ? typeName(sym->rtype()) : "void")+" "+sym->name()+"(";
  for(uint32_t i=0; i<sym->count(); ++i) {
    auto* p = vm.find_symbol_by_index(sym->index()+1+i);
    if(p==nullptr)
      break;
    if(i>0)
      text += ", ";
    text += typeName(p->type());
    text += " ";
    auto name = p->name();
    const auto dot = name.rfind('.');
    text += dot==std::string::npos ? name : name.substr(dot+1);
    }
  text += ")";
  return JS_NewString(ctx,text.c_str());
  }

/// debug.call(name, args, {self, other}) -> {ok, result, error}
/// The arguments are in the order of the declaration: numbers, strings, a character for a parameter of a class.
/// self and other are the characters of the call, as for a script of a dialog.
JSValue call(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue ret = JS_NewObject(ctx);
  auto fail = [&](const std::string& text) {
    JS_SetPropertyStr(ctx,ret,"ok",JS_FALSE);
    JS_SetPropertyStr(ctx,ret,"error",JS_NewString(ctx,text.c_str()));
    return ret;
    };

  auto* w = world();
  if(w==nullptr || argc<1)
    return fail("there is no game");
  auto& vm   = w->script().getVm();
  auto* sym  = vm.find_symbol_by_name(js::str(ctx,argv[0]));
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::FUNCTION)
    return fail("there is no such function");

  Npc* self  = nullptr;
  Npc* other = nullptr;
  if(argc>2 && JS_IsObject(argv[2])) {
    JSValue a = JS_GetPropertyStr(ctx,argv[2],"self");
    JSValue b = JS_GetPropertyStr(ctx,argv[2],"other");
    if(JS_IsObject(a)) self  = game::resolveNpc(ctx,a);
    if(JS_IsObject(b)) other = game::resolveNpc(ctx,b);
    JS_FreeValue(ctx,a);
    JS_FreeValue(ctx,b);
    }

  const uint32_t count = sym->count();
  JSValue list = argc>1 ? argv[1] : JS_UNDEFINED;
  std::vector<Npc*> characters(count,nullptr);
  // Validate instance arguments before putting anything on the VM stack.
  for(uint32_t i=0; i<count; ++i) {
    auto* parameter = vm.find_symbol_by_index(sym->index()+1+i);
    if(parameter==nullptr)
      return fail("invalid parameter symbol");
    if(parameter->type()!=zenkit::DaedalusDataType::INSTANCE)
      continue;
    JSValue value = JS_IsArray(list) ? JS_GetPropertyUint32(ctx,list,i) : JS_UNDEFINED;
    characters[i] = JS_IsObject(value) ? game::resolveNpc(ctx,value) : nullptr;
    JS_FreeValue(ctx,value);
    if(characters[i]==nullptr)
      return fail("parameter "+std::to_string(i+1)+" is a character");
    }
  std::shared_ptr<zenkit::DaedalusInstance> prevSelf, prevOther;
  auto* gs = vm.global_self();
  auto* go = vm.global_other();
  if(self!=nullptr)  { prevSelf  = gs->get_instance(); gs->set_instance(self->handlePtr()); }
  if(other!=nullptr) { prevOther = go->get_instance(); go->set_instance(other->handlePtr()); }
  auto restore = [&]() {
    if(self!=nullptr)  gs->set_instance(prevSelf);
    if(other!=nullptr) go->set_instance(prevOther);
    };

  try {
    for(uint32_t i=0; i<count; ++i) {
      auto* p = vm.find_symbol_by_index(sym->index()+1+i);
      JSValue v = JS_IsArray(list) ? JS_GetPropertyUint32(ctx,list,i) : JS_UNDEFINED;
      switch(p->type()) {
        case zenkit::DaedalusDataType::FLOAT: {
          double d = 0;
          JS_ToFloat64(ctx,&d,v);
          vm.push_float(float(d));
          break;
          }
        case zenkit::DaedalusDataType::STRING:
          vm.push_string(js::str(ctx,v));
          break;
        case zenkit::DaedalusDataType::INSTANCE: {
          vm.push_instance(characters[i]->handlePtr());
          break;
          }
        default: {
          int32_t d = 0;
          JS_ToInt32(ctx,&d,v);
          vm.push_int(d);
          break;
          }
        }
      JS_FreeValue(ctx,v);
      }

    vm.invoke_function(sym);
    JS_SetPropertyStr(ctx,ret,"ok",JS_TRUE);
    if(sym->has_return()) {
      switch(sym->rtype()) {
        case zenkit::DaedalusDataType::FLOAT:  JS_SetPropertyStr(ctx,ret,"result",JS_NewFloat64(ctx,double(vm.pop_float()))); break;
        case zenkit::DaedalusDataType::STRING: JS_SetPropertyStr(ctx,ret,"result",JS_NewString(ctx,vm.pop_string().c_str())); break;
        default:                               JS_SetPropertyStr(ctx,ret,"result",JS_NewInt32(ctx,vm.pop_int())); break;
        }
      }
    }
  catch(const std::exception& e) {
    restore();
    return fail(e.what());
    }
  restore();
  return ret;
  }

/// debug.symbol(name) -> {type, value} or null
JSValue symbol(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* w = world();
  if(w==nullptr || argc<1)
    return JS_NULL;
  auto* sym = w->script().getVm().find_symbol_by_name(js::str(ctx,argv[0]));
  if(sym==nullptr || sym->is_member())
    return JS_NULL;

  JSValue ret = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,ret,"type",JS_NewString(ctx,typeName(sym->type())));
  JS_SetPropertyStr(ctx,ret,"const",JS_NewBool(ctx,sym->is_const()));
  JS_SetPropertyStr(ctx,ret,"index",JS_NewUint32(ctx,sym->index()));
  try {
    switch(sym->type()) {
      case zenkit::DaedalusDataType::INT:    JS_SetPropertyStr(ctx,ret,"value",JS_NewInt32(ctx,sym->get_int()));              break;
      case zenkit::DaedalusDataType::FLOAT:  JS_SetPropertyStr(ctx,ret,"value",JS_NewFloat64(ctx,double(sym->get_float()))); break;
      case zenkit::DaedalusDataType::STRING: JS_SetPropertyStr(ctx,ret,"value",JS_NewString(ctx,sym->get_string().c_str()));  break;
      default: break;
      }
    }
  catch(const std::exception&) {
    // a symbol without a value
    }
  return ret;
  }

/// debug.setSymbol(name, value): true if it was set
JSValue setSymbol(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* w = world();
  if(w==nullptr || argc<2)
    return JS_FALSE;
  auto* sym = w->script().getVm().find_symbol_by_name(js::str(ctx,argv[0]));
  if(sym==nullptr || sym->is_member())
    return JS_FALSE;
  try {
    switch(sym->type()) {
      case zenkit::DaedalusDataType::INT: {
        int32_t v = 0;
        JS_ToInt32(ctx,&v,argv[1]);
        sym->set_int(v);
        return JS_TRUE;
        }
      case zenkit::DaedalusDataType::FLOAT: {
        double v = 0;
        JS_ToFloat64(ctx,&v,argv[1]);
        sym->set_float(float(v));
        return JS_TRUE;
        }
      case zenkit::DaedalusDataType::STRING:
        sym->set_string(js::str(ctx,argv[1]));
        return JS_TRUE;
      default: break;
      }
    }
  catch(const std::exception&) {
    }
  return JS_FALSE;
  }

/// debug.ikarus() -> what the layer for Ikarus and LeGo has to say, "" if the mod does not use them
JSValue ikarus(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  auto* w = world();
  return JS_NewString(ctx,w==nullptr ? "" : w->script().dmaReport().c_str());
  }

// --- the world

/// debug.placeOnGround(npc, waypoint, dx, dz) -> landing position or null.
/// Resolve each offset against terrain; a waypoint's Y is not valid nearby.
JSValue placeOnGround(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* npc = argc>0 ? game::resolveNpc(ctx,argv[0]) : nullptr;
  if(npc==nullptr || argc<2)
    return JS_NULL;
  auto& w = npc->world();
  auto* wp = w.findPoint(js::str(ctx,argv[1]),false);
  if(wp==nullptr)
    return JS_NULL;
  double dx = 0, dz = 0;
  if((argc>2 && JS_ToFloat64(ctx,&dx,argv[2])<0) ||
     (argc>3 && JS_ToFloat64(ctx,&dz,argv[3])<0))
    return JS_EXCEPTION;
  if(!std::isfinite(dx) || !std::isfinite(dz) || std::abs(dx)>2000 || std::abs(dz)>2000)
    return JS_NULL;

  const auto old = npc->position();
  const auto aim = wp->position()+Tempest::Vec3(float(dx),0,float(dz));
  for(int radius=0; radius<=200; radius+=50) {
    const int samples = radius==0 ? 1 : 8;
    for(int i=0; i<samples; ++i) {
      const float angle = float(i)*float(2*M_PI)/float(samples);
      auto p = aim+Tempest::Vec3(std::cos(angle)*float(radius),0,std::sin(angle)*float(radius));
      auto hit = w.physic()->landRay(p+Tempest::Vec3(0,500,0),1100);
      if(!hit.hasCol || hit.n.y<0.65f || std::abs(hit.v.y-aim.y)>500)
        continue;
      p.y = hit.v.y+5;
      npc->setPosition(p);
      const bool blocked = npc->hasCollision();
      npc->setPosition(old);
      if(blocked)
        continue;
      if(!npc->setInteraction(nullptr,true))
        return JS_NULL;
      npc->clearAiQueue();
      npc->clearSpeed();
      npc->stopAnim("");
      npc->setAnim(Npc::Anim::Idle);
      npc->setPosition(p);
      npc->updateTransform();
      if(npc->isPlayer()) {
        if(auto* camera = Gothic::inst().camera())
          camera->reset(npc);
        }
      JSValue ret = JS_NewObject(ctx);
      JS_SetPropertyStr(ctx,ret,"x",JS_NewFloat64(ctx,p.x));
      JS_SetPropertyStr(ctx,ret,"y",JS_NewFloat64(ctx,p.y));
      JS_SetPropertyStr(ctx,ret,"z",JS_NewFloat64(ctx,p.z));
      return ret;
      }
    }
  return JS_NULL;
  }

/// debug.parkNpc(npc): leave a disposable character under queue control only.
/// Unlike a puppet it still processes AI commands, including aiming/shooting.
JSValue parkNpc(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* npc = argc>0 ? game::resolveNpc(ctx,argv[0]) : nullptr;
  if(npc==nullptr || npc->isPlayer() || npc->isPuppet())
    return JS_FALSE;
  npc->clearState(true);
  npc->excRoutine(size_t(-1));
  npc->clearAiQueue();
  npc->setTarget(nullptr);
  npc->setOther(nullptr);
  npc->handlePtr()->daily_routine = 0;
  npc->handlePtr()->start_aistate  = 0;
  for(size_t i=0; i<PERC_Count; ++i)
    npc->setPerceptionDisable(PercType(i));
  npc->setAttitude(ATT_FRIENDLY);
  npc->setTempAttitude(ATT_FRIENDLY);
  return JS_TRUE;
  }

/// debug.queues(radius) -> ["name (distance): state ..., queue: ..."]: the characters near the player that are busy with a queue of
/// commands. For a scene that does not go on: who waits for whom.
JSValue queues(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue arr = JS_NewArray(ctx);
  auto*   w   = world();
  auto*   pl  = Gothic::inst().player();
  if(w==nullptr || pl==nullptr)
    return arr;
  double radius = 6000;
  if(argc>0)
    JS_ToFloat64(ctx,&radius,argv[0]);
  uint32_t n = 0;
  for(uint32_t i=0, c=w->npcCount(); i<c && n<60; ++i) {
    auto* npc = w->npcById(i);
    if(npc==nullptr)
      continue;
    const float dist = (npc->position()-pl->position()).length();
    if(double(dist)>radius || (npc->isAiQueueEmpty() && !npc->isPlayer() && npc->weaponState()==WeaponState::NoWeapon))
      continue;
    if(npc->isAiQueueEmpty() && !npc->isPlayer())
      continue;
    std::string line = std::string(npc->displayName()) + " (" + std::to_string(int(dist)) + "): " + npc->debugAiQueue();
    JS_SetPropertyUint32(ctx,arr,n++,JS_NewString(ctx,line.c_str()));
    }
  return arr;
  }

/// debug.faint(npc, on): knocks the character out (the same path as a blow of a human: the weapon drop, ZS_Unconscious) or
/// wakes him up
JSValue faint(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* npc = argc>0 ? game::resolveNpc(ctx,argv[0]) : nullptr;
  if(npc==nullptr)
    return JS_FALSE;
  npc->debugUnconscious(argc<2 || JS_ToBool(ctx,argv[1])>0);
  return JS_TRUE;
  }

/// debug.vobVisual(name, show, collide) -> [count, x, y, z] or null: what CHANGEVOBCOLLISION of the scripts does to the vobs of
/// the name; without the arguments the vob is only looked up
JSValue vobVisual(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* w = world();
  if(w==nullptr || argc<1)
    return JS_NULL;
  const std::string name = js::str(ctx,argv[0]);
  Tempest::Vec3 pos;
  if(!w->vobPosition(name,pos))
    return JS_NULL;
  size_t count = 1;
  if(argc>=3)
    count = w->setVobVisual(name,JS_ToBool(ctx,argv[1])>0,JS_ToBool(ctx,argv[2])>0);
  JSValue ret = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx,ret,0,JS_NewInt32(ctx,int32_t(count)));
  JS_SetPropertyUint32(ctx,ret,1,JS_NewFloat64(ctx,double(pos.x)));
  JS_SetPropertyUint32(ctx,ret,2,JS_NewFloat64(ctx,double(pos.y)));
  JS_SetPropertyUint32(ctx,ret,3,JS_NewFloat64(ctx,double(pos.z)));
  return ret;
  }

/// debug.setInfoKnown(info, known): reset/prepare the player's dialog history.
JSValue setInfoKnown(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* pl = Gothic::inst().player();
  if(pl==nullptr || argc<2)
    return JS_FALSE;
  return JS_NewBool(ctx,pl->world().script().setInfoKnown(*pl,js::str(ctx,argv[0]),JS_ToBool(ctx,argv[1])>0));
  }

/// debug.triggers(filter, max) -> [[name, kind, state], ...]: kind is "mover" or "trigger", the state of a mover is of zTMoverState
JSValue triggers(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue arr = JS_NewArray(ctx);
  auto*   w   = world();
  if(w==nullptr)
    return arr;

  const std::string filter = argc>0 ? js::str(ctx,argv[0]) : std::string();
  const size_t      max    = std::min<size_t>(number(ctx,argc,argv,1,200),2000);
  uint32_t          n      = 0;
  w->forEachTrigger([&](AbstractTrigger& t) {
    if(n>=max || t.name().empty() || !contains(t.name(),filter))
      return;
    JSValue e = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx,e,0,JS_NewString(ctx,std::string(t.name()).c_str()));
    if(auto* mv = dynamic_cast<MoveTrigger*>(&t)) {
      JS_SetPropertyUint32(ctx,e,1,JS_NewString(ctx,"mover"));
      JS_SetPropertyUint32(ctx,e,2,JS_NewInt32(ctx,mv->moverState()));
      } else {
      JS_SetPropertyUint32(ctx,e,1,JS_NewString(ctx,"trigger"));
      JS_SetPropertyUint32(ctx,e,2,JS_NewInt32(ctx,0));
      }
    JS_SetPropertyUint32(ctx,arr,n++,e);
    });
  return arr;
  }

/// debug.trigger(name, untrigger): sends the event to the vob, as Wld_SendTrigger and Wld_SendUntrigger of the scripts do
JSValue trigger(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* w = world();
  if(w==nullptr || argc<1)
    return JS_FALSE;
  const bool untrigger = argc>1 && JS_ToBool(ctx,argv[1])>0;
  w->triggerEvent(TriggerEvent(js::str(ctx,argv[0]),"",untrigger ? TriggerEvent::T_Untrigger : TriggerEvent::T_Trigger));
  return JS_TRUE;
  }

/// debug.points(filter, max) -> names of the way points and the free points
JSValue points(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue arr = JS_NewArray(ctx);
  auto*   w   = world();
  if(w==nullptr)
    return arr;

  const std::string filter = argc>0 ? js::str(ctx,argv[0]) : std::string();
  const size_t      max    = std::min<size_t>(number(ctx,argc,argv,1,200),2000);
  uint32_t          n      = 0;
  w->forEachPoint([&](const WayPoint& p) {
    if(n<max && !p.name.empty() && contains(p.name,filter))
      JS_SetPropertyUint32(ctx,arr,n++,JS_NewString(ctx,p.name.c_str()));
    });
  return arr;
  }

/// debug.god(on) and debug.marvin(on): the immortality of the player and the keys of the developer of the game
/// debug.containers(radius) -> nearby container vob names, nearest first.
JSValue containers(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue result = JS_NewArray(ctx);
  auto* w = world();
  auto* player = Gothic::inst().player();
  if(w==nullptr || player==nullptr)
    return result;
  const float radius = float(number(ctx,argc,argv,0,500));
  std::vector<std::pair<float,std::string>> found;
  for(uint32_t i=0; auto* mob = w->mobsiById(i); ++i) {
    const float distance = (mob->position()-player->position()).quadLength();
    if(mob->isContainer() && !mob->tag().empty() && distance<=radius*radius)
      found.emplace_back(distance,std::string(mob->tag()));
    }
  std::sort(found.begin(),found.end());
  for(uint32_t i=0; i<found.size() && i<100; ++i)
    JS_SetPropertyUint32(ctx,result,i,JS_NewString(ctx,found[i].second.c_str()));
  return result;
  }

/// debug.setLock(tag, code) -> {code, cracked} | null: the container gets the code of a lock (L and R) and is locked again,
/// an empty code takes the lock off. Without the tag it is the nearest container.
JSValue setLock(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* w = world();
  auto* player = Gothic::inst().player();
  if(w==nullptr || player==nullptr)
    return JS_NULL;
  const std::string tag  = argc>0 && JS_IsString(argv[0]) ? js::str(ctx,argv[0]) : std::string();
  const std::string code = argc>1 ? js::str(ctx,argv[1]) : std::string();
  Interactive* best = nullptr;
  float        dist = 0;
  for(uint32_t i=0; auto* mob = w->mobsiById(i); ++i) {
    if(!mob->isContainer())
      continue;
    if(!tag.empty() && !(mob->tag()==tag))
      continue;
    const float d = (mob->position()-player->position()).quadLength();
    if(best==nullptr || d<dist) {
      best = mob;
      dist = d;
      }
    }
  if(best==nullptr)
    return JS_NULL;
  best->setPickLockCode(code);
  best->setAsCracked(code.empty());
  JSValue o = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,o,"tag",    JS_NewString(ctx,std::string(best->tag()).c_str()));
  JS_SetPropertyStr(ctx,o,"code",   JS_NewString(ctx,std::string(best->pickLockCode()).c_str()));
  JS_SetPropertyStr(ctx,o,"cracked",JS_NewBool(ctx,best->isCracked()));
  return o;
  }

JSValue god(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc>0)
    Gothic::inst().setGodMode(JS_ToBool(ctx,argv[0])>0);
  return JS_NewBool(ctx,Gothic::inst().isGodMode());
  }

JSValue marvin(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc>0)
    Gothic::inst().setMarvinEnabled(JS_ToBool(ctx,argv[0])>0);
  return JS_NewBool(ctx,Gothic::inst().isMarvinEnabled());
  }

}

void game::installDebug(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue dbg    = JS_NewObject(ctx);
  js::fn(ctx,dbg,"log",       log,2);
  js::fn(ctx,dbg,"functions", functions,2);
  js::fn(ctx,dbg,"symbols",   symbols,2);
  js::fn(ctx,dbg,"signature", signature,1);
  js::fn(ctx,dbg,"call",      call,3);
  js::fn(ctx,dbg,"symbol",    symbol,1);
  js::fn(ctx,dbg,"setSymbol", setSymbol,2);
  js::fn(ctx,dbg,"ikarus",    ikarus,0);
  js::fn(ctx,dbg,"triggers",  triggers,2);
  js::fn(ctx,dbg,"trigger",   trigger,2);
  js::fn(ctx,dbg,"points",    points,2);
  js::fn(ctx,dbg,"containers",containers,1);
  js::fn(ctx,dbg,"parkNpc",   parkNpc,1);
  js::fn(ctx,dbg,"faint",     faint,2);
  js::fn(ctx,dbg,"queues",    queues,1);
  js::fn(ctx,dbg,"vobVisual", vobVisual,3);
  js::fn(ctx,dbg,"placeOnGround",placeOnGround,4);
  js::fn(ctx,dbg,"setInfoKnown",setInfoKnown,2);
  js::fn(ctx,dbg,"setLock",   setLock,2);
  js::fn(ctx,dbg,"god",       god,1);
  js::fn(ctx,dbg,"marvin",    marvin,1);
  JS_SetPropertyStr(ctx,global,"debug",dbg);
  JS_FreeValue(ctx,global);
  }
