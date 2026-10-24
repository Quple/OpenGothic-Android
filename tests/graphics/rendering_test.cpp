#include "graphics/renderdistance.h"
#include "graphics/drawclusters.h"
#include "graphics/instancestorage.h"
#include "graphics/shaders.h"

#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>

// These CPU regressions must never initialize or submit work to a GPU.
Resources* Resources::inst = nullptr;
void Resources::recycle(Tempest::StorageBuffer&&) { std::abort(); }
Shaders& Shaders::inst(bool) { std::abort(); }

class InstanceStorageTest {
  public:
    static void submit(InstanceStorage& storage, int32_t frame) {
      storage.join();
      {
      std::lock_guard<std::mutex> guard(storage.sync);
      storage.uploadFId = frame;
      }
      storage.uploadCnd.notify_one();
      }

    static void run() {
      {
      InstanceStorage storage;
      auto id = storage.alloc(2*sizeof(Tempest::Matrix4x4));
      Tempest::Matrix4x4 matrices[2];
      matrices[0].identity();
      matrices[1].identity();
      id.set(matrices);
      std::fill(storage.durty.begin(),storage.durty.end(),0);
      id.set(matrices);
      for(const auto mask:storage.durty)
        assert(mask==0); // unchanged bones must not generate uploads
      matrices[1].set(3,0,25.f);
      id.set(matrices);
      const auto changed = id.offsetId<Tempest::Matrix4x4>()+1;
      for(size_t i=0; i<storage.blockCnt; ++i)
        assert(bool(storage.durty[i/32] & (1u<<(i%32)))==(i==changed));
      }
      // Empty transfers exercise the real worker and completion handshake
      // without requiring a GPU. Submit immediately to cover early notify.
      for(int i=0; i<100; ++i) {
        InstanceStorage storage;
        for(int j=0; j<100; ++j) {
          submit(storage,j%Resources::MaxFramesInFlight);
          storage.join();
          }
        submit(storage,0); // destruction must drain pending work
        }
      }
  };

static void distances() {
  using namespace RenderDistance;
  assert(landscape(0)==5000.f);
  assert(landscape(4)==25000.f);
  assert(landscape(14)==75000.f);
  assert(landscape(-100)==landscape(0));
  assert(landscape(100)==landscape(14));
  const auto choices = percentChoices("20%|40%|60%|80%|100%|120%|140%|160%|180%|200%");
  assert(choices.size()==10);
  assert(objects(choiceScale(4,choices),landscape(4))==10000.f);
  assert(objects(choiceScale(0,choices),landscape(4))==2000.f);
  assert(objects(choiceScale(9,choices),landscape(4))==20000.f);
  assert(objects(choiceScale(3,{}),landscape(4))==25000.f);
  assert(objects(choiceScale(1,{}),landscape(4))==10000.f);
  assert(choiceScale(-1,choices)==choices.front());
  assert(choiceScale(100,choices)==choices.back());
  assert(percentChoices("Distance# 25 %|100%|250%").size()==3);
  assert(std::abs(percentChoices("12.5%|100%").front()-0.125f)<0.000001f);
  for(auto text:{"off|on", "20%|", "nan%", "-10%", "junk%", "0%", ".%", "1.2.3%"})
    assert(percentChoices(text).empty());
  assert(objects(std::numeric_limits<float>::quiet_NaN(),25000.f)==10000.f);
  assert(sphereVisible(11000.f*11000.f,1000.f,10000.f));
  assert(!sphereVisible(11001.f*11001.f,1000.f,10000.f));
  assert(sphereVisible(15000.f*15000.f,5000.f,10000.f));
  assert(sphereVisible(1e10f,0,0));
  }

static void scaledBounds() {
  using Matrix = std::array<std::array<float,3>,3>;
  const Matrix identity = {{{1,0,0},{0,1,0},{0,0,1}}};
  assert(RenderDistance::maxScale(identity)==1.f);
  auto scaled = identity;
  scaled[0][0] = -4;
  assert(RenderDistance::maxScale(scaled)==4.f);
  std::mt19937 random(25);
  std::uniform_real_distribution<float> value(-3.f,3.f);
  for(int i=0; i<10000; ++i) {
    Matrix matrix;
    for(auto& column:matrix)
      for(auto& element:column)
        element = value(random);
    const float scale = RenderDistance::maxScale(matrix);
    std::array<float,3> direction = {value(random),value(random),value(random)};
    float before = 0, after = 0;
    for(int row=0; row<3; ++row) {
      before += direction[row]*direction[row];
      float v = 0;
      for(int column=0; column<3; ++column)
        v += matrix[column][row]*direction[column];
      after += v*v;
      }
    assert(after<=before*scale*scale+0.001f);
    }
  }

static void clusterReuse() {
  DrawClusters storage;
  std::array<PackedMesh::Cluster,40> clusters{};
  const auto first = storage.alloc(clusters.data(),0,10,0,0);
  storage.alloc(clusters.data(),0,1,0,0); // separate the two holes
  const auto second = storage.alloc(clusters.data(),0,20,0,0);
  storage.alloc(clusters.data(),0,1,0,0);
  storage.free(first,10);
  storage.free(second,20);
  const auto reused = storage.alloc(clusters.data(),0,8,0,0);
  assert(reused>=first && reused+8<=first+10);
  assert(storage.size()==32);
  }

int main() {
  distances();
  scaledBounds();
  clusterReuse();
  InstanceStorageTest::run();
  std::cout << "PASS: distances, scaled bounds, cluster reuse, 10000 upload handshakes\n";
  }
