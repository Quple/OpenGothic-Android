#include "rainfx.h"

#include <zenkit/addon/daedalus.hh>
#include <Tempest/Log>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

#include "graphics/pfx/particlefx.h"
#include "physics/dynamicworld.h"
#include "utils/androidinput.h"
#include "resources.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "camera.h"
#include "gothic.h"

using namespace Tempest;

static uint32_t mix(uint32_t x) {
  x ^= x>>16;
  x *= 0x7feb352dU;
  x ^= x>>15;
  x *= 0x846ca68bU;
  x ^= x>>16;
  return x;
  }

/// The particle effects are not in the scripts (the original draws the rain by the engine), so they are built here.
/// The drops: the streaks of SKYRAIN.TGA fall from the box over the camera, stretched along the velocity. The drops are absolute (they
/// stay in the world when the camera moves).
struct DropDesc final {
  float pps;
  float box;          // half of the side of the box over the camera, cm
  float width;        // cm
  float length;       // cm
  float vel;          // cm/ms
  float velVar;
  float alphaStart;   // 0..255
  float alphaEnd;
  const char* color;
  };

static std::unique_ptr<ParticleFx> makeDropFx(const DropDesc& d, const char* name) {
  zenkit::IParticleEffect src;
  src.pps_value             = d.pps;
  src.pps_is_looping        = 1;
  src.pps_fps               = 1;

  src.shp_type_s            = "BOX";
  src.shp_for_s             = "WORLD";
  // the drops appear at all the heights from 2 m to 15 m over the camera and fall for 1.1 s (11 m): the rain is seen high over the head too,
  // not only in the 8 m under the place where every drop was born
  src.shp_offset_vec_s      = "0 850 0";
  src.shp_is_volume         = 1;
  {
    char dim[64];
    std::snprintf(dim,sizeof(dim),"%d 650 %d",int(d.box),int(d.box));
    src.shp_dim_s           = dim;
  }

  src.dir_mode_s            = "DIR";
  src.dir_for_s             = "WORLD";
  src.dir_angle_head        = 30;
  src.dir_angle_head_var    = 15;
  src.dir_angle_elev        = -84;
  src.dir_angle_elev_var    = 1.5f;

  src.vel_avg               = d.vel;
  src.vel_var               = d.velVar;
  src.lsp_part_avg          = 1100;
  src.lsp_part_var          = 100;
  src.fly_gravity_s         = "0 0 0";

  src.vis_name_s            = "SKYRAIN.TGA";
  src.vis_orientation_s     = "VELO3D_FIXED"; // the width stays when the drop flies at the camera: the rain is seen when the player looks up
  src.vis_tex_is_quadpoly   = 1;
  src.vis_tex_color_start_s = d.color;
  src.vis_tex_color_end_s   = d.color;
  {
    char size[64];
    std::snprintf(size,sizeof(size),"%d %d",int(d.width),int(d.length));
    src.vis_size_start_s    = size;
  }
  src.vis_size_end_scale    = 1;
  src.vis_alpha_func_s      = "BLEND";
  src.vis_alpha_start       = d.alphaStart;
  src.vis_alpha_end         = d.alphaEnd;
  src.use_emitters_for      = 0;

  return std::unique_ptr<ParticleFx>(new ParticleFx(src,name));
  }

/// The drops that hit the ground and bounce off it: a few short streaks fly up from a patch of the ground and fall back (a flat box
/// of 1.4 m, the gravity of the earth, ~320 ms). The emitters are put on the ground by RainFx (a ray down), the particles are absolute.
static std::unique_ptr<ParticleFx> makeSplashFx() {
  zenkit::IParticleEffect src;
  src.pps_value             = 14;
  src.pps_is_looping        = 1;
  src.pps_fps               = 1;

  src.shp_type_s            = "BOX";
  src.shp_for_s             = "WORLD";
  src.shp_offset_vec_s      = "0 5 0";
  src.shp_is_volume         = 1;
  src.shp_dim_s             = "70 0 70";

  src.dir_mode_s            = "DIR";
  src.dir_for_s             = "WORLD";
  src.dir_angle_head        = 0;
  src.dir_angle_head_var    = 179;   // the direction on the ground is random (180 and more is ignored by the particle system)
  src.dir_angle_elev        = 76;
  src.dir_angle_elev_var    = 14;

  src.vel_avg               = 0.16f;
  src.vel_var               = 0.05f;
  src.lsp_part_avg          = 320;
  src.lsp_part_var          = 50;
  src.fly_gravity_s         = "0 -0.001 0";

  src.vis_name_s            = "SKYRAIN.TGA";
  src.vis_orientation_s     = "VELO3D";
  src.vis_tex_is_quadpoly   = 1;
  src.vis_tex_color_start_s = "235 240 250";
  src.vis_tex_color_end_s   = "235 240 250";
  src.vis_size_start_s      = "3 8";
  src.vis_size_end_scale    = 1;
  src.vis_alpha_func_s      = "BLEND";
  src.vis_alpha_start       = 118;   // half of it: the bounces of the heavy rain were too strong
  src.vis_alpha_end         = 40;
  src.use_emitters_for      = 0;

  std::unique_ptr<ParticleFx> ret(new ParticleFx(src,"RAIN_SPLASH"));
  // the streak of SKYRAIN.TGA is a thin line: at 6 cm it is not seen. A soft blob (made here) stretched along the velocity is.
  ret->visMaterial.tex = &Resources::rainDropTexture();
  return ret;
  }

RainFx::RainFx(World& owner)
  :owner(owner) {
  // the near drops are thick and fast, in a small box: the heavy rain right in front of the eyes.
  // the far drops are thinner, in a wide box: the rain that covers the distance.
  declNear   = makeDropFx({850.f,  550.f, 6.f, 125.f, 1.05f, 0.10f, 225.f, 150.f, "215 225 245"},"RAIN_DROP_NEAR");
  declFar    = makeDropFx({1450.f, 1800.f, 8.f, 110.f, 0.95f, 0.10f, 200.f, 115.f, "205 215 235"},"RAIN_DROP_FAR");
  declSplash = makeSplashFx();
  rainCover  = std::make_shared<RainCover>();
  declNear->cover = rainCover.get();
  declFar ->cover = rainCover.get();
  settingsTimer = 1000; // read the settings at the first tick
  }

RainFx::~RainFx() {
  }

/// Only the outdoor worlds of the original games have the rain
bool RainFx::isOutdoorWorld(std::string_view name) {
  std::string up(name);
  for(auto& c:up)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  return up.rfind("NEWWORLD",0)==0 || up.rfind("OLDWORLD",0)==0 || up.rfind("ADDONWORLD",0)==0 || up=="WORLD.ZEN" || up=="WORLD";
  }

/// The weather of the day: some days have a rain of 40 to 240 minutes, with the fade of 15 minutes at both ends. The day tells
/// everything, so the weather is the same after a save and a load, and the first day is dry.
RainFx::Plan RainFx::planOf(int64_t day) {
  Plan ret;
  if(day<1)
    return ret;

  const uint32_t h = mix(uint32_t(day)*2654435761u + 0x9e3779b9u);
  if(h%100>=35)
    return ret;

  ret.rain  = true;
  ret.start = 5*60 + int((h>>8)%(15*60));   // from 5:00 to 20:00
  ret.len   = 40 + int((h>>16)%200);        // from 40 to 240 minutes
  ret.peak  = ((h>>24)%3==0) ? 1.f : 0.85f; // the storm or the strong rain: every rain is a real one
  return ret;
  }

/// How much of the rain the setting of the player lets through: 0 off, 1 light, 2 normal, 3 heavy (everything, 6 layers of the drops)
float RainFx::strength(int level) {
  static const float k[4] = {0.f, 0.3f, 0.6f, 1.f};
  return k[std::clamp(level,0,3)];
  }

/// The clouds of the weather (the sky of the storm): the light rain has a thin cover, the normal one a thick one, the heavy one is the storm.
/// A rain is a storm fast: a part of the weather is enough for all of the cover.
float RainFx::overcast() const {
  static const float cover[4] = {0.f, 0.5f, 0.78f, 1.f};
  return std::min(1.f, weather*1.2f)*cover[std::clamp(level,0,3)];
  }

float RainFx::scheduled() const {
  if(forced>=0)
    return forced>0 ? 1.f : 0.f;

  const gtime t    = owner.time();
  const Plan  plan = planOf(t.day());
  if(!plan.rain)
    return 0;

  const int now = int(t.hour())*60 + int(t.minute());
  if(now<plan.start || now>=plan.start+plan.len)
    return 0;

  const float fade = 15.f;
  const float k    = std::min(1.f, std::min(float(now-plan.start), float(plan.start+plan.len-now))/fade);
  return k*plan.peak;
  }

float RainFx::rnd() {
  rndState ^= rndState<<13;
  rndState ^= rndState>>17;
  rndState ^= rndState<<5;
  return float(rndState&0xFFFFFFu)/float(0x1000000u);
  }

/// What is over the player (the sound of the rain is muffled by it; the drops and the wet ground have the grid of RainCover): five rays up
/// (the center and 2.5 m around it, 30 m high), the share of them that hits a roof. One ray was a switch: a branch over the head was a roof.
void RainFx::updateShelter(const Vec3& cam) {
  // The player counts, not the camera: the camera of the third person swings around the player, goes through the walls and under the branches,
  // and the rain and the reflections were off for a turn of the head.
  Vec3 pos = cam;
  if(auto* pl = owner.player())
    pos = pl->position()+Vec3(0,170,0);

  float roof = 0;
  if(auto* dyn = owner.physic()) {
    static const Vec3 around[5] = {{0,0,0}, {250,0,0}, {-250,0,0}, {0,0,250}, {0,0,-250}};
    int hit = 0;
    for(const auto& a:around)
      if(dyn->ray(pos+a+Vec3(0,30,0), pos+a+Vec3(0,3000,0)).hasCol)
        ++hit;
    roof = float(hit)/5.f;
    }
  // one ray of five (a branch) is the open sky, four of five is a roof
  roofShare = std::clamp((roof-0.2f)/0.6f, 0.f, 1.f);
  const bool room = !owner.roomAt(pos).empty();
  covered = roofShare>0.5f || room;
  inRoom  = room;
  }

/// The highest surface over every cell of the grid around the camera (a ray from 35 m over the camera down): the ground where the sky is open,
/// a roof where it is not. Only the cells that came into the window of the grid are made (at most 96 rays in a tick), the rest is kept.
void RainFx::updateCover(const Vec3& cam) {
  auto* dyn = owner.physic();
  if(dyn==nullptr)
    return;

  constexpr int N     = RainCover::N;
  constexpr int total = N*N;
  const int     minX  = RainCover::cellOf(cam.x) - N/2;
  const int     minZ  = RainCover::cellOf(cam.z) - N/2;

  int budget = 96;
  int s      = coverScan;
  for(int k=0; k<total && budget>0; ++k, s=(s+1)%total) {
    const int i  = s/N, j = s%N;
    // the cell of the window that the slot keeps
    const int cx = minX + ((i-minX)&(N-1));
    const int cz = minZ + ((j-minZ)&(N-1));
    auto& slot = rainCover->slot[size_t(s)];
    if(int(slot.cx)==cx && int(slot.cz)==cz)
      continue;
    --budget;

    const float x = (float(cx)+0.5f)*RainCover::Cell;
    const float z = (float(cz)+0.5f)*RainCover::Cell;
    const auto  r = dyn->ray(Vec3(x,cam.y+3500.f,z), Vec3(x,cam.y-2500.f,z));
    float top  = r.hasCol ? r.v.y : RainCover::Open;
    // the crown of a tree: the rain ends at the top of it, the ground under it is wet (it is not a roof of the physics)
    float crown = RainCover::Open;
    for(const auto& c:owner.treeCrowns()) {
      const float dx = x-c.at.x, dz = z-c.at.z;
      if(dx*dx+dz*dz<c.radius*c.radius && c.top>crown && c.at.y<cam.y+3500.f && c.top>r.v.y+150.f)
        crown = c.top;
      }
    slot.cx   = float(cx);
    slot.cz   = float(cz);
    slot.top  = top;
    slot.crown = crown;
    ++rainCover->version;
    }
  coverScan = s;
  }
void RainFx::updateSound(const Vec3& pos, float volume) {
  if(noise.isEmpty()) {
    if(volume<0.02f)
      return;
    noise = ::Sound(owner,::Sound::T_Regular,"RAIN_01.WAV",pos,100000.f,false);
    noise.setAmbient(true);
    noise.setLooping(true);
    noise.play();
    }
  noise.setPosition(pos);
  noise.setVolume(volume);
  }

/// A place for the bounce: a random point around the camera, the ground under it (a ray down), and nothing over it (a ray up: a roof,
/// a bridge, a tree take the rain away). The walls and the steep slopes get no bounces.
bool RainFx::pickSplash(const Vec3& cam, Vec3& out) {
  auto* dyn = owner.physic();
  if(dyn==nullptr)
    return false;

  const float a = rnd()*2.f*float(M_PI);
  const float r = 80.f + 1150.f*rnd(); // more of them near the camera
  const float x = cam.x + std::cos(a)*r;
  const float z = cam.z + std::sin(a)*r;

  const auto down = dyn->ray(Vec3(x,cam.y+250.f,z), Vec3(x,cam.y-1200.f,z));
  if(!down.hasCol) {
    ++missNoGround;
    return false;
    }
  if(down.n.y!=0.f && std::abs(down.n.y)<0.45f) { // the normal of the objects is not known (it is zero); the sign depends on the winding
    ++missSteep;
    return false;
    }

  const auto up = dyn->ray(down.v+Vec3(0,20,0), down.v+Vec3(0,1800,0));
  if(up.hasCol) {
    ++missRoof;
    return false;
    }
  // the roofs without the collision and the crowns of the trees (RainCover): no drops bounce there either
  if(rainCover->topAt(down.v.x,down.v.z) > down.v.y+150.f) {
    ++missRoof;
    return false;
    }
  ++placed;

  out = down.v;
  return true;
  }

/// The bounces of the drops: the emitters sit on the ground around the camera and move to another place from time to time.
void RainFx::updateSplashes(const Vec3& cam, size_t want, uint64_t dt) {
  splashLogTimer += dt;
  if(splashLogTimer>=10000) {
    splashLogTimer = 0;
    if(want>0)
      Log::d("[rain] bounces: wanted ",want,", places found ",placed,", no ground ",missNoGround,", steep ",missSteep,", roof ",missRoof," (the last 10 s)");
    placed = missNoGround = missSteep = missRoof = 0;
    }
  size_t budget = 4; // the places to find in one tick (2 rays each)

  for(size_t i=0; i<SplashCount; ++i) {
    auto& s = splash[i];

    if(i>=want || splashFailed) {
      if(!s.fx.isEmpty() && s.fx.isActive())
        s.fx.setActive(false);
      s.wait = 0;
      continue;
      }

    if(s.fx.isEmpty()) {
      s.fx = PfxEmitter(owner,declSplash.get());
      if(s.fx.isEmpty()) {
        splashFailed = true;
        Log::e("[rain] the emitter of the bounces is empty: the texture SKYRAIN.TGA is not loaded");
        continue;
        }
      s.fx.setLooped(true);
      s.wait = 0;
      }

    if(s.wait>dt) {
      s.wait -= dt;
      continue;
      }
    if(budget==0)
      continue;
    --budget;

    Vec3 at;
    if(pickSplash(cam,at)) {
      s.fx.setPosition(at);
      s.fx.setActive(true);
      s.wait = 450 + uint64_t(rnd()*700.f);
      } else {
      s.fx.setActive(false);
      s.wait = 120;
      }
    }
  }

void RainFx::tick(uint64_t dt) {
  auto* cam = Gothic::inst().camera();
  if(cam==nullptr)
    return;
  const Vec3 pos = cam->originLwc();

  settingsTimer += dt;
  if(settingsTimer>=1000) {
    settingsTimer = 0;
    const auto pref = AndroidInput::preferences();
    level      = std::clamp(pref.rain,0,3);
    wetSetting = std::clamp(pref.wetGround,0,2);
    outdoor = isOutdoorWorld(owner.name());
    if(level!=loggedLevel || outdoor!=loggedOutdoor) {
      loggedLevel   = level;
      loggedOutdoor = outdoor;
      Log::d("[rain] world ",owner.name(),", the setting ",level," (0 off .. 3 heavy), outdoor world ",outdoor ? "yes" : "no");
      }
    }

  // the plan of the day: when it rains is in the log
  const int64_t day = owner.time().day();
  if(day!=loggedDay) {
    loggedDay = day;
    const Plan plan = planOf(day);
    if(plan.rain)
      Log::d("[rain] day ",day,": the rain from ",plan.start/60,":",(plan.start%60)/10,plan.start%10," for ",plan.len," minutes (strength ",plan.peak,")");
    else
      Log::d("[rain] day ",day,": dry");
    }

  shelterTimer += dt;
  if(shelterTimer>=250) {
    shelterTimer = 0;
    updateShelter(pos);
    }
  updateCover(pos);

  // the weather comes and goes slowly
  const float target = (level>0 && outdoor) ? scheduled() : 0.f;
  const float step   = float(dt)/8000.f;
  weather += std::clamp(target-weather,-step,step);

  const float cover = std::min(1.f, float(dt)/500.f);
  shelter += ((inRoom ? 1.f : roofShare)-shelter)*cover;

  // what the player sees and hears: the weather without the roofs, with the setting of the player
  const float visible = weather*(1.f-shelter);
  const float k       = strength(level);

  // the ground: the rain wets it in ~20 s, without the rain it dries in ~3 minutes (at once when the rain is off in the settings)
  {
    const float cap = 0.3f + 0.7f*k;
    if(level==0 || !outdoor || wetSetting==0)
      wet = std::max(0.f, wet-float(dt)/2000.f);
    else if(weather>0.15f)
      wet = std::min(cap, wet+float(dt)/20000.f*weather);
    else
      wet = std::max(0.f, wet-float(dt)/180000.f);
    wet = std::min(wet,cap);
    fall    = weather*k; // the rings on the puddles do not stop under a roof
  }

  // the drops and the bounces are everywhere around the camera: the ones under a roof end by themselves (RainCover), the weather does not depend on the player
  const size_t count  = size_t(std::clamp(int(std::floor(weather*k*float(LayerCount)+0.5f)),0,int(LayerCount)));
  const size_t bounce = size_t(std::clamp(int(std::floor(weather*k*float(SplashCount)+0.5f)),0,int(SplashCount)));

  if(count!=loggedCount || bounce!=loggedSplash || covered!=loggedCovered) {
    loggedCount   = count;
    loggedSplash  = bounce;
    loggedCovered = covered;
    Log::d("[rain] layers ",count," of ",LayerCount,", bounces ",bounce," of ",SplashCount,", the weather ",weather," (target ",target,
           "), the shelter ",shelter,covered ? " (a roof or a room is over the camera)" : "",", at ",pos.x," ",pos.y," ",pos.z);
    }

  for(size_t i=0; i<LayerCount; ++i) {
    if(layer[i].isEmpty()) {
      if(i>=count)
        continue;
      // the near and the far layers go one after another
      layer[i] = PfxEmitter(owner,(i%2==0 ? declNear : declFar).get());
      if(layer[i].isEmpty()) {
        static bool once = false;
        if(!once) {
          once = true;
          Log::e("[rain] the emitter of the drops is empty: the texture SKYRAIN.TGA is not loaded");
          }
        continue;
        }
      layer[i].setLooped(true);
      }
    layer[i].setPosition(pos);
    layer[i].setActive(i<count);
    }

  updateSplashes(pos,bounce,dt);

  wetLogTimer += dt;
  if(wetLogTimer>=5000) {
    wetLogTimer = 0;
    if(wet>0.01f)
      Log::d("[rain] wet ground ",wet," (the player is in a room: ",inRoom ? "yes" : "no","), the rain ",fall);
    }

  updateSound(pos, std::min(1.f, visible*(0.3f+0.7f*k))*0.85f);
  }
