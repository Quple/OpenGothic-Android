#include "graphics/texturequality.h"
#include "graphics/mesh/submesh/progressivemesh.h"
#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/shaders.h"
#include <Tempest/MemReader>
#include <zenkit/Stream.hh>
#include <cassert>
#include <iostream>
#include <limits>

const Shaders::Options& Shaders::options() {
  static Options options;
  return options;
  }

static void textures() {
  using namespace TextureQuality;
  assert(mipSkip(1)==0 && mipSkip(0)==3 && mipSkip(0.5f)==2);
  assert(mipSkip(-10)==3 && mipSkip(10)==0);
  assert(mipSkip(std::numeric_limits<float>::quiet_NaN())==0);
  Tempest::Pixmap pm(32,16,Tempest::TextureFormat::RGBA8);
  auto* pixels = static_cast<uint8_t*>(pm.data());
  for(size_t i=0; i<pm.dataSize(); i+=4) {
    pixels[i]=uint8_t((i/4)%32);
    pixels[i+1]=100;
    pixels[i+2]=200;
    pixels[i+3]=255;
    }
  auto half = reduce(pm,1);
  assert(half.w()==16 && half.h()==8 && half.dataSize()==pm.dataSize()/4);
  auto* small = static_cast<const uint8_t*>(half.data());
  assert(small[0]==1 && small[1]==100 && small[2]==200 && small[3]==255);
  auto low = reduce(pm,3);
  assert(low.w()==8 && low.h()==4); // preserve non-square dimensions

  // Real BC1 DDS layout: verify payload and mip count survive top-mip removal.
  std::vector<uint8_t> dds(128+128+32+8);
  const auto put = [&](size_t at, uint32_t v) {
    for(size_t i=0; i<4; ++i) dds[at+i]=uint8_t(v>>(8*i));
    };
  std::memcpy(dds.data(),"DDS ",4);
  put(4,124); put(12,16); put(16,16); put(28,3); put(84,0x31545844);
  std::fill(dds.begin()+128,dds.begin()+256,0x11);
  std::fill(dds.begin()+256,dds.begin()+288,0x22);
  std::fill(dds.begin()+288,dds.end(),0x33);
  assert(reduceDds(dds,1));
  assert(dds.size()==168 && dds[128]==0x22 && dds[160]==0x33);
  Tempest::MemReader reader(dds.data(),dds.size());
  Tempest::Pixmap compressed(reader);
  assert(compressed.w()==8 && compressed.h()==8 && compressed.mipCount()==2);
  assert(compressed.format()==Tempest::TextureFormat::DXT1 && compressed.dataSize()==40);
  assert(reduceDds(dds,3));
  assert(dds.size()==136 && dds[128]==0x33);
  assert(!reduceDds(dds,3));
  dds.resize(130);
  assert(!reduceDds(dds,3));
  }

static zenkit::MultiResolutionMesh mesh() {
  zenkit::MultiResolutionMesh mesh;
  mesh.positions = {{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0.5f,0.5f,0}};
  mesh.sub_meshes.resize(1);
  auto& sm = mesh.sub_meshes[0];
  for(uint16_t i=0; i<5; ++i)
    sm.wedges.push_back({{0,0,1},{mesh.positions[i].x,mesh.positions[i].y},i});
  sm.wedge_map = {65535,65535,65535,65535,0};
  sm.triangles = {{{0,1,4}},{{1,2,4}},{{2,3,4}},{{3,0,4}}};
  return mesh;
  }

static size_t triangles(const zenkit::MultiResolutionMesh& mesh) {
  size_t count=0;
  for(const auto& sm:mesh.sub_meshes) {
    count+=sm.triangles.size();
    for(const auto& t:sm.triangles) {
      const auto a=sm.wedges.at(t.wedges[0]).index;
      const auto b=sm.wedges.at(t.wedges[1]).index;
      const auto c=sm.wedges.at(t.wedges[2]).index;
      assert(a<mesh.positions.size() && b<mesh.positions.size() && c<mesh.positions.size());
      assert(a!=b && a!=c && b!=c);
      }
    }
  return count;
  }

int main(int argc, char** argv) {
  textures();
  auto original=mesh(), low=original;
  assert(ProgressiveMesh::simplify(low,0.35f));
  assert(triangles(low)==2 && triangles(original)==4);
  // The two remaining triangles cover the same unit square, without holes.
  float area=0;
  for(const auto& t:low.sub_meshes[0].triangles) {
    const auto& sm=low.sub_meshes[0];
    const auto a=low.positions[sm.wedges[t.wedges[0]].index];
    const auto b=low.positions[sm.wedges[t.wedges[1]].index];
    const auto c=low.positions[sm.wedges[t.wedges[2]].index];
    area+=((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))*0.5f;
    }
  assert(area==1.f);
  auto bad=original;
  bad.sub_meshes[0].wedge_map[4]=4;
  assert(!ProgressiveMesh::simplify(bad,0.35f));
  bad=original;
  bad.sub_meshes[0].wedge_map.clear();
  assert(!ProgressiveMesh::simplify(bad,0.35f));
  size_t native=0, reduced=0, fullMeshlets=0, lowMeshlets=0;
  for(int i=1; i<argc; ++i) {
    zenkit::MultiResolutionMesh data;
    auto reader=zenkit::Read::from(argv[i]);
    data.load(reader.get());
    const auto full=triangles(data);
    PackedMesh packed(data,PackedMesh::PK_Visual);
    if(native==0) {
      zenkit::SoftSkinMesh skin;
      skin.mesh=data;
      skin.weights.resize(data.positions.size());
      for(size_t v=0; v<data.positions.size(); ++v) {
        skin.weights[v].push_back({0.75f,data.positions[v],0});
        skin.weights[v].push_back({0.25f,data.positions[v],1});
        }
      PackedMesh animated(skin);
      assert(animated.verticesA.size()==packed.vertices.size());
      assert(animated.indices==packed.indices && animated.indices8==packed.indices8);
      for(size_t v=0; v<packed.vertices.size(); ++v)
        for(size_t c=0; c<3; ++c)
          assert(animated.verticesA[v].pos[0][c]==packed.vertices[v].pos[c]);
      }
    for(const auto& sub:packed.subMeshes) {
      const auto count=sub.iboLength/PackedMesh::MaxInd;
      fullMeshlets+=count;
      lowMeshlets+=sub.lod[1].firstMeshlet!=0 ? sub.lod[1].meshletCount : count;
      for(const auto& lod:sub.lod) {
        if(lod.firstMeshlet==0) continue;
        assert(lod.meshletCount<=count);
        assert((lod.firstMeshlet+lod.meshletCount)*PackedMesh::MaxInd<=packed.indices.size());
        for(size_t m=lod.firstMeshlet; m<lod.firstMeshlet+lod.meshletCount; ++m)
          for(size_t t=0; t<PackedMesh::MaxInd; ++t) {
            const auto index=packed.indices[m*PackedMesh::MaxInd+t];
            assert(index>=m*PackedMesh::MaxVert && index<(m+1)*PackedMesh::MaxVert);
            }
        }
      }
    assert(packed.vertices.size()/PackedMesh::MaxVert==packed.indices.size()/PackedMesh::MaxInd);
    assert(packed.indices8.size()/(PackedMesh::MaxPrim*4)==packed.indices.size()/PackedMesh::MaxInd);
    for(float level:{0.65f,0.35f}) {
      auto lod=data;
      if(ProgressiveMesh::simplify(lod,level)) {
        assert(triangles(lod)<full);
        ++reduced;
        }
      }
    ++native;
    }
  std::cout << "Texture and progressive LOD regressions: PASS; native meshes=" << native
            << ", reduced levels=" << reduced << ", meshlets full/low=" << fullMeshlets << '/' << lowMeshlets << '\n';
  }
