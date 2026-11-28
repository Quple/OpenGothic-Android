#pragma once

#include <array>
#include <memory>
#include <string>
#include <cstdint>

#include <Tempest/Vec>

#include "world/objects/pfxemitter.h"
#include "world/objects/sound.h"
#include "world/raincover.h"

class World;
class ParticleFx;

/// The rain of the outdoor worlds: the drops around the camera, the drops that bounce off the ground, the noise of the rain and the weather of the day.
/// The original game has the rain (skyEffects=1 of Gothic.ini); here the player can set its strength in the settings of the touch interface.
class RainFx final {
  public:
    RainFx(World& owner);
    ~RainFx();

    void  tick(uint64_t dt);

    /// The weather that is on now (0..1), the same for the whole world: it darkens the sky, the scripts ask for it (Wld_IsRaining)
    float overcast() const;
    bool  isRaining() const { return weather>0.2f; }
    /// How wet the ground is (0..1): the rain wets it in ~20 s, it dries in ~3 minutes. The rooms stay dry (dryZones). The renderer draws the reflections.
    float wetness() const { return wet; }
    /// What is over the ground around the camera (the roofs): the drops end there, the ground under them is dry
    std::shared_ptr<const RainCover> cover() const { return rainCover; }
    /// The rain that falls over the camera now (0..1): the rings on the puddles
    float rainFall() const { return fall; }
    /// The setting of the player for the wet ground: 0 off, 1 darker only, 2 with the reflections
    int   wetMode() const { return wetSetting; }
    /// 1: the rain is on, 0: the rain is off, -1: the weather of the day
    void  force(int mode) { forced = mode; }

  private:
    static constexpr size_t LayerCount  = 6;    // the layers of the falling drops: the near ones (thick, fast) and the far ones (a wide box)
    static constexpr size_t SplashCount = 36;   // the places on the ground around the camera where the drops bounce off

    struct Splash final {
      PfxEmitter fx;
      uint64_t   wait = 0;   // ms until the place is changed
      };

    static bool  isOutdoorWorld(std::string_view name);
    struct Plan final {
      bool  rain  = false;
      int   start = 0;      // minutes of the day
      int   len   = 0;      // minutes
      float peak  = 0;
      };
    static Plan  planOf(int64_t day);
    static float strength(int level);
    float        scheduled() const;
    float        rnd();
    bool         pickSplash(const Tempest::Vec3& cam, Tempest::Vec3& out);
    void         updateSplashes(const Tempest::Vec3& cam, size_t want, uint64_t dt);
    void         updateShelter(const Tempest::Vec3& cam);
    void         updateCover(const Tempest::Vec3& cam);
    void         updateSound(const Tempest::Vec3& pos, float volume);

    World&                              owner;
    std::unique_ptr<ParticleFx>         declNear, declFar, declSplash;
    std::array<PfxEmitter,LayerCount>   layer;
    std::array<Splash,SplashCount>      splash;
    Sound                               noise;

    bool                                outdoor    = false;
    int                                 wetSetting = 2;
    int                                 level      = 0;      // the setting of the player: 0 off, 1 light, 2 normal, 3 heavy
    int                                 forced     = -1;     // Marvin: 1 on, 0 off, -1 the weather of the day
    float                               weather    = 0;      // the weather of the world, smoothed
    float                               shelter    = 0;      // 1: a roof or a room is over the camera
    bool                                covered    = false;
    float                               roofShare  = 0;       // 0 open sky .. 1 a roof (the rays up)
    bool                                inRoom     = false;   // a room of the world is around the camera (not only a roof)
    std::shared_ptr<RainCover>          rainCover;
    int                                 coverScan  = 0;
    float                               wet        = 0;       // the wet ground, before the rooms
    float                               fall       = 0;       // the rain over the camera
    uint32_t                            rndState   = 0x2545f491u;
    bool                                splashFailed = false;
    uint32_t                            placed = 0, missNoGround = 0, missSteep = 0, missRoof = 0; // the places of the bounces, for the log
    uint64_t                            splashLogTimer = 0;
    uint64_t                            wetLogTimer    = 0;
    uint64_t                            settingsTimer = 0;
    uint64_t                            shelterTimer  = 0;
    int64_t                             loggedDay  = -1;
    size_t                              loggedCount = size_t(-1);
    size_t                              loggedSplash = size_t(-1);
    int                                 loggedLevel = -1;
    bool                                loggedOutdoor = false;
    bool                                loggedCovered = false;
  };
