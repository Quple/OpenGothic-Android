#include "physicmeshshape.h"

#include "graphics/mesh/submesh/packedmesh.h"
#include "dynamicworld.h"

float PhysicMeshShape::friction() const {
  return frict;
  }

PhysicMeshShape* PhysicMeshShape::load(PackedMesh&& packed) {
  // Render LODs are appended after the original mesh. Collision uses only
  // the original ranges and should not retain their extra CPU vertices.
  size_t originalIndices=0;
  bool hasLods=false;
  for(const auto& sub:packed.subMeshes) {
    originalIndices = std::max(originalIndices,sub.iboOffset+sub.iboLength);
    hasLods |= sub.lod[0].firstMeshlet!=0 || sub.lod[1].firstMeshlet!=0;
    }
  if(hasLods) {
    packed.vertices.resize((originalIndices/PackedMesh::MaxInd)*PackedMesh::MaxVert);
    packed.indices.resize(originalIndices);
    packed.indices.shrink_to_fit();
    }
  uint32_t count = 0;
  for(auto& i : packed.subMeshes)
    if(!i.material.disable_collision && i.iboLength > 0) {
      count++;
      }
  if(count == 0)
    return nullptr;
  return new PhysicMeshShape(std::move(packed));
  }

PhysicMeshShape::PhysicMeshShape(PackedMesh&& sPacked)
  :mesh(std::move(sPacked)), shape(&mesh, mesh.useQuantization(), true) {
  for(auto& i:sPacked.subMeshes)
    frict += DynamicWorld::materialFriction(i.material.group);
  frict = frict/float(sPacked.subMeshes.size());
  }
