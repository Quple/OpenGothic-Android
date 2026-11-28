#pragma once

#include <array>
#include <cmath>
#include <cstdint>

/// What is over the ground around the camera: for every cell of the grid (1.2 m) the height of the highest surface that a ray from the sky meets
/// (the ground itself where the sky is open, a roof, a bridge or a ceiling where it is not). A drop that falls below it is under a roof and it
/// is gone, the ground below it is dry. The grid is a ring of the cells of the world: a step of the camera changes a row of them only, and the
/// stored cell tells which cell of the world a slot keeps, so a slot of the old place is never taken for the new one.
class RainCover final {
  public:
    static constexpr int   N       = 64;                   // the cells in a side (a power of 2)
    static constexpr float Cell    = 120.f;                // cm
    static constexpr float Open    = -1.0e9f;              // nothing is known: the sky is open

    struct Slot final {
      float top = Open;     // the height of the highest surface (cm)
      float cx  = 1.0e9f;   // the cell of the world that the slot keeps
      float cz  = 1.0e9f;
      float pad  = 0;
      float crown = Open;   // the top of the crown of a tree over the place: the rain ends there, the ground under it is wet
      float pad0 = 0;       // the slot is two vec4 of the shader
      float pad1 = 0;
      float pad2 = 0;
      };

    static int cellOf(float v) { return int(std::floor(v/Cell)); }
    static int slotOf(int cx, int cz) { return (cx&(N-1))*N + (cz&(N-1)); }

    /// The height of the highest thing that the rain does not fall through (a roof, a crown of a tree), Open when it is not known: the drops and the bounces end there
    float topAt(float x, float z) const {
      const int cx = cellOf(x), cz = cellOf(z);
      const Slot& s = slot[size_t(slotOf(cx,cz))];
      if(int(s.cx)!=cx || int(s.cz)!=cz)
        return Open;
      return s.top>s.crown ? s.top : s.crown;
      }

    /// The height of the highest surface of the physics (not the crowns of the trees): what the ground under it is dry for
    float roofAt(float x, float z) const {
      const int cx = cellOf(x), cz = cellOf(z);
      const Slot& s = slot[size_t(slotOf(cx,cz))];
      if(int(s.cx)!=cx || int(s.cz)!=cz)
        return Open;
      return s.top;
      }

    std::array<Slot,size_t(N*N)> slot;
    uint32_t                     version = 0;   // changes with every cell that is made again
  };
