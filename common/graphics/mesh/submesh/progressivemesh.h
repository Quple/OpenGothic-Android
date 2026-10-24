#pragma once

#include <zenkit/MultiResolutionMesh.hh>
#include <algorithm>
#include <cmath>

namespace ProgressiveMesh {

// MRM vertices are ordered by the author's collapse sequence. Resolve the
// wedge map rather than discarding triangles, preserving material/UV seams.
inline bool simplify(zenkit::MultiResolutionMesh& mesh, float fraction) {
  if(!std::isfinite(fraction))
    return false;
  size_t vertexCount = size_t(std::ceil(double(mesh.positions.size())*std::clamp(fraction, 0.f, 1.f)));
  for(const auto& sm:mesh.sub_meshes) {
    if(sm.wedge_map.size()!=sm.wedges.size())
      return false;
    for(size_t i=0; i<sm.wedges.size(); ++i) {
      if(sm.wedges[i].index>=mesh.positions.size())
        return false;
      if(sm.wedge_map[i]==uint16_t(-1))
        vertexCount = std::max(vertexCount, size_t(sm.wedges[i].index)+1);
      }
    }

  bool changed = false;
  for(auto& sm:mesh.sub_meshes) {
    std::vector<uint16_t> remap(sm.wedges.size());
    for(size_t i=0; i<remap.size(); ++i) {
      size_t next = i, steps = 0;
      while(sm.wedges[next].index>=vertexCount) {
        next = sm.wedge_map[next];
        if(next>=sm.wedges.size() || ++steps>sm.wedges.size())
          return false; // malformed mod data: caller retains full detail
        }
      remap[i] = uint16_t(next);
      }
    size_t count = 0;
    for(auto tri:sm.triangles) {
      for(auto& w:tri.wedges) {
        if(w>=remap.size())
          return false;
        w = remap[w];
        }
      const auto a = sm.wedges[tri.wedges[0]].index;
      const auto b = sm.wedges[tri.wedges[1]].index;
      const auto c = sm.wedges[tri.wedges[2]].index;
      if(a==b || a==c || b==c)
        continue;
      sm.triangles[count++] = tri;
      }
    changed |= count<sm.triangles.size();
    sm.triangles.resize(count);
    }
  return changed;
  }

}
