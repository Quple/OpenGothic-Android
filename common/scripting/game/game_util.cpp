#include "scripting/gameapi.h"

#include "game/gamescript.h"
#include "world/objects/npc.h"
#include "world/world.h"

using namespace Scripting;

/// Looking up of characters and of the names of script instances

int64_t game::npcIndex(World& world, const Npc& npc) {
  const uint32_t n = world.npcCount();
  for(uint32_t i=0; i<n; ++i) {
    if(world.npcById(i)==&npc)
      return int64_t(i);
    }
  return -1;
  }

std::string game::instanceName(World& world, size_t symbol) {
  if(auto* s = world.script().findSymbol(symbol))
    return std::string(s->name());
  return std::string();
  }

size_t game::findInstance(World& world, std::string_view name) {
  if(name.empty())
    return size_t(-1);
  return world.script().findSymbolIndex(name);
  }
