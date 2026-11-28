#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Texture2d>

#include "graphics/lightsource.h"
#include "graphics/sceneglobals.h"
#include "resources.h"
#include "world/raincover.h"
#include <memory>

class World;
class gtime;

class Sky final {
  public:
    struct State final {
      const Tempest::Texture2d* lay[2] = {};
      };

    Sky(std::string_view wname);
    ~Sky();

    void updateLight(const gtime gameTime);

    const LightSource&        sunLight()         const { return sun;     }
    const Tempest::Vec3&      ambientLight()     const { return ambient; }
    float                     sunIntensity()     const { return GSunIntensity;  }
    float                     moonIntensity()    const { return GMoonIntensity; }

    const Tempest::Texture2d& sunImage()  const { return *sunImg;  }
    const Tempest::Texture2d& moonImage() const { return *moonImg; }

    const State&              cloudsDay()   const { return clouds[0]; }
    // while it rains the second night layer (it is nearly invisible) is SKYRAINCLOUDS.TGA, the dark clouds of the game: the shader takes the storm from it
    const State&              cloudsNight() const { return (overcast>0.02f && rainClouds!=nullptr) ? stormNight : clouds[1]; }
    Tempest::Vec2             cloudsOffset(int layer, uint64_t tickCount) const;
    float                     isNight() const;

    /// The clouds of the rain (0..1): the sun and the sky light are weaker
    void                      setOvercast(float v) { overcast = v; }
    float                     overcastLevel() const { return overcast; }

    /// What is over the ground around the camera (the roofs): the renderer does not draw the wet ground under them
    void                      setCover(std::shared_ptr<const RainCover> c) { rainCover = std::move(c); }
    const RainCover*          cover() const { return rainCover.get(); }

    /// The ground after the rain: how wet it is (0..1) and how much rain falls over it now (0..1): the renderer draws the reflections
    void                      setWetness(float wet, float rain, int mode) { wetGround = wet; rainNow = rain; wetMode = mode; }
    /// The setting of the player: 1 the ground is darker only, 2 it reflects too
    int                       wetnessMode() const { return wetMode; }
    float                     wetness() const { return wetGround; }
    float                     rainFall() const { return rainNow; }

  private:
    const Tempest::Texture2d*     skyTexture(std::string_view name, bool day, size_t id);
    const Tempest::Texture2d*     implSkyTexture(std::string_view name, bool day, size_t id);

    LightSource                   sun;
    Tempest::Vec3                 ambient;

    State                         clouds[2]; //day, night;
    State                         stormNight;
    const Tempest::Texture2d*     rainClouds = nullptr;

    const Tempest::Texture2d*     sunImg   = &Resources::fallbackBlack();
    const Tempest::Texture2d*     moonImg  = &Resources::fallbackBlack();

    float                         overcast       = 0;
    float                         wetGround      = 0;
    float                         rainNow        = 0;
    int                           wetMode        = 2;
    std::shared_ptr<const RainCover> rainCover;
    float                         GSunIntensity  = 0;
    float                         GMoonIntensity = 0;
  };
