#include "graphics/drawclusters.h"
#include "graphics/sceneglobals.h"
#include <Tempest/VulkanApi>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <cassert>
#include <iostream>

int main(int argc, char** argv) {
  using namespace Tempest;
  assert(argc==2);
  VulkanApi api{ApiFlags::Validation};
  Device device(api);
  auto shader=device.shader(argv[1]);
  auto pipeline=device.pipeline(shader);
  SceneGlobals::UboGlobal scene;
  scene.camPos=Vec3(100,0,0);
  for(auto& plane:scene.frustrum)
    plane=Vec4(0);
  auto ubo=device.ssbo(&scene,sizeof(scene));
  uint32_t zero[16]={};
  auto instance=device.ssbo(zero,sizeof(zero));
  auto bucket=device.ssbo(zero,sizeof(zero));
  auto output=device.ssbo(Uninitialized,10*16);
  auto hiZ=device.texture(Pixmap(1,1,TextureFormat::RGBA8));
  struct Push {
    uint32_t firstMeshlet=0,meshletCount=1;
    float znear=0,minObjectPixels=0;
    Vec2 viewport;
    float objectDistance=0,modelDetail=0;
    } push;
  static_assert(sizeof(Push)==32);
  const auto check = [&](float distance, float radius, float quality, bool terrain,
                          bool emptyLod, float range, uint32_t expectedCount, uint32_t expectedFirst) {
    DrawClusters::Cluster cluster;
    cluster.pos=Vec3(100,0,distance);
    cluster.r=radius;
    cluster.firstMeshlet=100;
    cluster.meshletCount=10;
    cluster.instanceId=terrain ? uint32_t(-1) : 0;
    cluster.lod={MeshLod{200,5},MeshLod{300,emptyLod ? 0u : 2u}};
    auto clusters=device.ssbo(&cluster,sizeof(cluster));
    uint32_t draw[5]={};
    auto indirect=device.ssbo(draw,sizeof(draw));
    push.modelDetail=quality;
    push.objectDistance=range;
    auto command=device.commandBuffer();
    {
    auto enc=command.startEncoding(device);
    enc.setBinding(0,ubo);
    enc.setBinding(1,output);
    enc.setBinding(2,instance);
    enc.setBinding(3,bucket);
    enc.setBinding(4,indirect);
    enc.setBinding(5,clusters);
    enc.setBinding(7,hiZ,Sampler::bilinear());
    enc.setPushData(push);
    enc.setPipeline(pipeline);
    enc.dispatch(1);
    }
    auto fence=device.submit(command);
    fence.wait();
    device.readBytes(indirect,draw,sizeof(draw));
    assert(draw[1]==expectedCount);
    if(expectedCount>0) {
      uint32_t payload[40]={};
      device.readBytes(output,payload,sizeof(payload));
      for(uint32_t i=0; i<expectedCount; ++i) {
        assert(payload[i*4]==cluster.instanceId);
        assert(payload[i*4+1]==expectedFirst+i);
        }
      }
    };
  check(3,1,0,false,false,0,10,100);
  check(6,1,0,false,false,0,5,200);
  check(13,1,0,false,false,0,2,300);
  check(13,1,1,false,false,0,10,100);
  check(40,1,1,false,false,0,5,200);
  check(100,1,1,false,false,0,2,300);
  check(13,3,0,false,false,0,10,100);
  check(13,1,0,true,false,0,10,100);
  check(13,1,0,false,true,0,0,0);
  check(13,1,0,false,false,10,0,0);
  std::cout << "Vulkan LOD selection and cluster layout: PASS (" << device.properties().name << ")\n";
  }
