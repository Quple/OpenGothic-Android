#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

namespace RenderDistance {

// Engine distances in Gothic world units (100 units per metre).
inline float landscape(int sightValue) {
  return 25000.f*(float(std::clamp(sightValue,0,14))+1.f)*0.2f;
  }

inline float objects(float scale, float landscapeDistance) {
  if(!std::isfinite(scale))
    scale = 1.f;
  return std::min(10000.f*std::clamp(scale,0.2f,10.f),landscapeDistance);
  }

inline bool sphereVisible(float distanceSquared, float radius, float range) {
  const float limit = range + std::max(radius,0.f);
  return range<=0 || distanceSquared<=limit*limit;
  }

// Upper bound on the linear transform's largest singular value. Unlike
// the longest column, this also bounds shear (for example wind matrices).
template<class Matrix>
float maxScale(const Matrix& matrix) {
  float bound = 0;
  for(int i=0; i<3; ++i) {
    float row = 0;
    for(int j=0; j<3; ++j) {
      float dot = 0;
      for(int k=0; k<3; ++k)
        dot += matrix[i][k]*matrix[j][k];
      row += std::abs(dot);
      }
    bound = std::max(bound,row);
    }
  return std::sqrt(bound);
  }

// Some mod menus store a choice index where vanilla uses a multiplier.
// Read the actual percentages, including non-uniform choice sequences.
inline std::vector<float> percentChoices(std::string_view text) {
  std::vector<float> result;
  if(const auto hash = text.find('#'); hash!=std::string_view::npos)
    text.remove_prefix(hash+1);
  while(!text.empty()) {
    const auto end = text.find('|');
    auto token = text.substr(0,end);
    while(!token.empty() && token.front()==' ')
      token.remove_prefix(1);
    while(!token.empty() && token.back()==' ')
      token.remove_suffix(1);
    if(token.empty() || token.back()!='%')
      return {};
    token.remove_suffix(1);
    while(!token.empty() && token.back()==' ')
      token.remove_suffix(1);
    float percent = 0, divisor = 1;
    bool decimal = false, digit = false;
    for(const char c:token) {
      if(c=='.' && !decimal) {
        decimal = true;
        continue;
        }
      if(c<'0' || c>'9')
        return {};
      digit = true;
      percent = percent*10.f+float(c-'0');
      if(decimal)
        divisor *= 10.f;
      }
    percent /= divisor;
    if(!digit || !std::isfinite(percent) || percent<=0)
      return {};
    result.push_back(percent*0.01f);
    if(end==std::string_view::npos)
      break;
    text.remove_prefix(end+1);
    if(text.empty())
      return {};
    }
  return result;
  }

inline float choiceScale(float value, const std::vector<float>& choices) {
  if(!std::isfinite(value))
    return 1.f;
  if(choices.empty())
    return value;
  const auto index = size_t(std::clamp(value,0.f,float(choices.size()-1)));
  return choices[index];
  }

}
