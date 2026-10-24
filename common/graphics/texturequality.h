#pragma once

#include <Tempest/Pixmap>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace TextureQuality {

inline uint32_t mipSkip(float quality) {
  if(!std::isfinite(quality))
    return 0;
  return uint32_t(std::lround((1.f-std::clamp(quality,0.f,1.f))*3.f));
  }

inline uint32_t mipLevel(uint32_t w, uint32_t h, uint32_t count, uint32_t skip) {
  uint32_t level = 0;
  while(level<skip && level+1<count && w>=8 && h>=8) {
    w /= 2;
    h /= 2;
    ++level;
    }
  return level;
  }

// Drop existing compressed mips without decoding/recompressing BC textures.
// DDS offsets refer to the standard 124-byte DDSURFACEDESC2 header.
inline bool reduceDds(std::vector<uint8_t>& data, uint32_t skip) {
  if(skip==0 || data.size()<128 || std::memcmp(data.data(),"DDS ",4)!=0)
    return false;
  const auto get = [&](size_t offset) {
    uint32_t v = 0;
    for(size_t i=0; i<4; ++i)
      v |= uint32_t(data[offset+i])<<(8*i);
    return v;
    };
  const auto put = [&](size_t offset, uint32_t v) {
    for(size_t i=0; i<4; ++i)
      data[offset+i] = uint8_t(v>>(8*i));
    };
  if(get(4)!=124)
    return false;
  const auto fourCC = get(84);
  const uint32_t block = fourCC==0x31545844 ? 8 :
                        fourCC==0x33545844 || fourCC==0x35545844 ? 16 : 0;
  uint32_t w=get(16), h=get(12), count=std::max(1u,get(28));
  if(block==0 || w==0 || h==0 || count>32)
    return false;
  const uint32_t level = mipLevel(w,h,count,skip);
  if(level==0)
    return false;
  size_t offset = 128;
  for(uint32_t i=0; i<level; ++i) {
    const uint64_t bytes = uint64_t((uint64_t(w)+3)/4)*((uint64_t(h)+3)/4)*block;
    if(bytes>data.size()-offset)
      return false;
    offset += size_t(bytes);
    w = std::max(1u,w/2);
    h = std::max(1u,h/2);
    }
  if(offset>=data.size())
    return false;
  std::memmove(data.data()+128,data.data()+offset,data.size()-offset);
  data.resize(data.size()-(offset-128));
  put(12,h);
  put(16,w);
  put(20,((w+3)/4)*((h+3)/4)*block);
  put(28,count-level);
  return true;
  }

inline Tempest::Pixmap reduce(Tempest::Pixmap pm, uint32_t skip) {
  const auto format = pm.format();
  if(format!=Tempest::TextureFormat::R8 && format!=Tempest::TextureFormat::RG8 &&
     format!=Tempest::TextureFormat::RGB8 && format!=Tempest::TextureFormat::RGBA8)
    return pm;
  for(uint32_t level=0; level<skip && pm.w()>=8 && pm.h()>=8; ++level) {
    Tempest::Pixmap small(pm.w()/2,pm.h()/2,format);
    const auto channels = pm.bpp();
    auto* src = static_cast<const uint8_t*>(pm.data());
    auto* dst = static_cast<uint8_t*>(small.data());
    for(uint32_t y=0; y<small.h(); ++y)
      for(uint32_t x=0; x<small.w(); ++x)
        for(uint32_t c=0; c<channels; ++c) {
          const size_t at = (size_t(y*2)*pm.w()+x*2)*channels+c;
          dst[(size_t(y)*small.w()+x)*channels+c] = uint8_t((uint32_t(src[at])+src[at+channels]+
              src[at+pm.w()*channels]+src[at+(pm.w()+1)*channels]+2)/4);
          }
    pm = std::move(small);
    }
  return pm;
  }

}
