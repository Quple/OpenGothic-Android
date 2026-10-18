#include "scripting/gameapi.h"

#include <map>

#include "game/gamescript.h"
#include "game/questlog.h"
#include "gothic.h"
#include "world/world.h"

using namespace Scripting;

namespace {

QuestLog* log() {
  auto* world = Gothic::inst().world();
  // the log of the game is exposed read-only, the scripts of the game write to it the same way
  return world!=nullptr ? const_cast<QuestLog*>(&world->script().questLog()) : nullptr;
  }

/// The scripts of the game name quests by their title. A mod has a short id for it.
std::map<std::string,std::string>& titles() {
  static std::map<std::string,std::string> t;
  return t;
  }

std::string titleOf(JSContext* ctx, JSValueConst id) {
  const auto key = js::mod(ctx).manifest.id+":"+js::str(ctx,id);
  auto it = titles().find(key);
  return it!=titles().end() ? it->second : js::str(ctx,id);
  }

/// quests.create(id, title, section) - section 0 is a mission, 1 is a note
JSValue create(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* ql = log();
  if(ql==nullptr || argc<2)
    return JS_FALSE;

  titles()[js::mod(ctx).manifest.id+":"+js::str(ctx,argv[0])] = js::str(ctx,argv[1]);
  int32_t section = 0;
  if(argc>2)
    JS_ToInt32(ctx,&section,argv[2]);
  ql->add(js::str(ctx,argv[1]),section==1 ? QuestLog::Section::Note : QuestLog::Section::Mission);
  return JS_TRUE;
  }

JSValue addEntry(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* ql = log();
  if(ql==nullptr || argc<2)
    return JS_FALSE;
  ql->addEntry(titleOf(ctx,argv[0]),js::str(ctx,argv[1]));
  return JS_TRUE;
  }

JSValue setStatus(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* ql = log();
  if(ql==nullptr || argc<2)
    return JS_FALSE;

  int32_t st = 1;
  JS_ToInt32(ctx,&st,argv[1]);
  if(st<1 || st>4)
    return JS_FALSE;
  ql->setStatus(titleOf(ctx,argv[0]),QuestLog::Status(st));
  return JS_TRUE;
  }

}

/// quests: the journal. create, addEntry, setStatus and the constants of the status.
void game::installQuests(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue quests = JS_NewObject(ctx);
  js::fn(ctx,quests,"create",   create,3);
  js::fn(ctx,quests,"addEntry", addEntry,2);
  js::fn(ctx,quests,"setStatus",setStatus,2);

  JSValue status = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,status,"RUNNING", JS_NewInt32(ctx,int(QuestLog::Status::Running)));
  JS_SetPropertyStr(ctx,status,"SUCCESS", JS_NewInt32(ctx,int(QuestLog::Status::Success)));
  JS_SetPropertyStr(ctx,status,"FAILED",  JS_NewInt32(ctx,int(QuestLog::Status::Failed)));
  JS_SetPropertyStr(ctx,status,"OBSOLETE",JS_NewInt32(ctx,int(QuestLog::Status::Obsolete)));
  JS_SetPropertyStr(ctx,quests,"status",status);

  JS_SetPropertyStr(ctx,global,"quests",quests);
  JS_FreeValue(ctx,global);
  }
