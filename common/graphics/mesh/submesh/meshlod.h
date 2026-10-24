#pragma once

#include <array>
#include <cstdint>

struct MeshLod {
  uint32_t firstMeshlet = 0;
  uint32_t meshletCount = 0;
  };

using MeshLods = std::array<MeshLod, 2>;
