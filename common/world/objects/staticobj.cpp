#include "staticobj.h"

#include <Tempest/Log>

#include "world/visualfilter.h"
#include "world/world.h"
#include "utils/string_frm.h"

using namespace Tempest;

/// The meshes of the trees that have a crown (not the stumps, the trunks, the roots and the dead trees): the rain does not fall under it
static bool isTreeWithCrown(std::string_view name) {
  std::string up(name);
  for(auto& c:up)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  if(up.find("TREE")==std::string::npos && up.find("BAUM")==std::string::npos)
    return false;
  static const char* no[] = {"STUMP","STAMM","ROOT","DEAD","DESTROY","FALLEN","FIRE","LOG","BRANCH"};
  for(auto* n:no)
    if(up.find(n)!=std::string::npos)
      return false;
  return true;
  }

StaticObj::StaticObj(Vob* parent, World& world, const zenkit::VirtualObject& vob, Flags flags)
  : Vob(parent, world, vob, flags) {
  visual.setVisual(vob,world,(flags & Flags::Static));
  visual.setObjMatrix(transform());

  scheme = vob.visual->name;
  shown  = vob.show_visual;

  // the crown of a tree: a disc in the bounding box of the mesh (the physics does not have the leaves, so the rain falls through them)
  if(!scheme.empty() && isTreeWithCrown(scheme)) {
    const auto& b = vob.bbox;
    const float w = b.max.x-b.min.x, d = b.max.z-b.min.z, h = b.max.y-b.min.y;
    if(h>350.f && w>220.f && d>220.f)
      world.addTreeCrown(Vec3(0.5f*(b.min.x+b.max.x), b.min.y, 0.5f*(b.min.z+b.max.z)), 0.40f*std::max(w,d), b.max.y);
    }

  // the world is made while a mod that hides some visuals is running: they are not drawn from the start
  if(!scheme.empty() && VisualFilter::inst().hides(scheme)) {
    filtered = true;
    visual.setShown(false,-1,world,transform());
    }
  }

void StaticObj::setVobVisual(bool show, bool collideDynamic) {
  shown = show;
  visual.setShown(shown && !filtered,collideDynamic ? 1 : 0,world,transform());
  }

void StaticObj::filterVisual() {
  if(scheme.empty())
    return;
  const bool want = VisualFilter::inst().hides(scheme);
  if(want==filtered)
    return;
  filtered = want;
  visual.setShown(shown && !filtered,-1,world,transform());
  }

void StaticObj::visualInfo(const std::function<void(std::string_view,bool)>& f) const {
  if(!scheme.empty())
    f(scheme,filtered);
  }

void StaticObj::moveEvent() {
  Vob::moveEvent();
  visual.setObjMatrix(transform());
  }

bool StaticObj::setMobState(std::string_view sc, int32_t st) {
  const bool ret = Vob::setMobState(sc,st);

  if(scheme.find(sc)!=0)
    return ret;
  string_frm name("S_S",st);
  if(visual.startAnimAndGet(name,world.tickCount())!=nullptr) {
    // state = st;
    return ret;
    }
  return false;
  }
