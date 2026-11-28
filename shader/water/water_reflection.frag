#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "scene.glsl"
#include "common.glsl"

layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };

#include "gerstner_wave.glsl"

layout(binding = 1) uniform sampler2D  sceneColor;
layout(binding = 2) uniform sampler2D  gbufDiffuse;
layout(binding = 3) uniform usampler2D gbufNormal;
layout(binding = 4) uniform sampler2D  gbufDepth;
layout(binding = 5) uniform sampler2D  zbuffer;

layout(binding =  6) uniform sampler2D skyLUT;

#if defined(WET)
// The ground that is wet after the rain: the reflection of the sky and of the scene (SSR) over the ground that looks up. Drawn over
// the scene (the reflection is added, the ground gets darker where it is wet), only when it is raining or the ground is still wet.
layout(push_constant, std430) uniform UboPush {
  float wet;      // 0..1: how wet the ground is
  float rain;     // 0..1: the rain that falls now (the rings on the puddles)
  float skyMul;   // the sky under the clouds of the rain is darker
  } push;

// What is over the ground around the camera (RainCover of the CPU, the same ring of 64x64 cells of 1.2 m): the highest surface of every cell.
// The ground under a roof (a hut, a porch, a bridge) is dry, it is seen through the doors and the windows too. The ground under the crown of a tree
// is wet and reflects, only the rain does not fall there (no rings on the puddles).
layout(binding = 7, std430) readonly buffer UboCover {
  vec4 cell[]; // two for a cell: (top, the cell of the world x, z, 0), (the top of a crown of a tree, 0, 0, 0)
  } cover;

// x: a roof is over the place (dry), y: a crown of a tree is over it (the rain does not fall)
vec2 wetCovered(vec3 p) {
  const ivec2 c = ivec2(floor(p.xz/1.2e2));
  const int   i = ((c.x&63)*64 + (c.y&63))*2;
  const vec4  e = cover.cell[i];
  if(int(e.y)!=c.x || int(e.z)!=c.y)
    return vec2(0.0); // the cell is not known (far away or not made yet): the sky is open
  return vec2((e.x > p.y + 150.0) ? 1.0 : 0.0, (cover.cell[i+1].x > p.y + 150.0) ? 1.0 : 0.0);
  }
float wetHash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx)*0.1031);
  p3 += dot(p3, p3.yzx+33.33);
  return fract((p3.x+p3.y)*p3.z);
  }

vec2 wetHash22(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx)*vec3(0.1031,0.1030,0.0973));
  p3 += dot(p3, p3.yzx+33.33);
  return fract((p3.xx+p3.yz)*p3.zy);
  }

float wetNoise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f*f*(3.0-2.0*f);
  return mix(mix(wetHash12(i),             wetHash12(i+vec2(1,0)),f.x),
             mix(wetHash12(i+vec2(0,1)),   wetHash12(i+vec2(1,1)),f.x), f.y);
  }

// the puddles: the places of the world (cm) where the water stays; the wetter the ground, the more of them
float wetPuddle(vec2 w, float wet) {
  float n = wetNoise(w/380.0)*0.6 + wetNoise(w/130.0)*0.3 + wetNoise(w/45.0)*0.1;
  float t = mix(0.80, 0.52, wet);
  return smoothstep(t, t+0.07, n);
  }

// the rings of the drops on the water: a slope that is added to the normal of the puddle
vec2 wetRipples(vec2 w, float t, float rain) {
  vec2 sum = vec2(0);
  for(int k=0; k<2; ++k) {
    const float cell = (k==0) ? 34.0 : 23.0;
    const vec2  p    = w/cell + vec2(float(k)*7.31, float(k)*3.17);
    const vec2  id   = floor(p);
    const vec2  f    = fract(p);
    const vec2  h    = wetHash22(id);

    const float x    = t*(0.8+0.4*h.x) + h.y*9.0;
    const float cyc  = floor(x);
    const float ph   = x-cyc;
    const vec2  c    = 0.2 + 0.6*wetHash22(id + vec2(cyc*1.7, cyc*0.9));
    const float on   = step(wetHash12(id + vec2(cyc*3.1, 11.3)), rain);

    const vec2  d2   = f-c;
    const float d    = length(d2);
    const float r    = ph*0.42;
    const float edge = min(min(f.x,1.0-f.x), min(f.y,1.0-f.y));
    const float win  = smoothstep(0.0, 0.1, edge);
    const float dr   = (d-r)*14.0;
    const float env  = exp(-dr*dr) * (1.0-ph)*(1.0-ph) * win * on;
    if(d>1e-4)
      sum += (d2/d) * env * sin((d-r)*45.0);
    }
  return sum*0.5;
  }
#endif
float intersectPlane(const vec3 pos, const vec3 dir, const vec4 plane) {
  float dist = dot(vec4(pos,1.0), plane);
  float step = -dot(plane.xyz,dir);
  if(abs(step)<=0.0)
    return 0;
  return dist/step;
  }

float intersectFrustum(const vec3 pos, const vec3 dir) {
  float ret = 0;
  for(int i=0; i<6; ++i) {
    // skip left and right planes to hide some SSR artifacts
    if(i==0 || i==1)
      continue;
    float len = intersectPlane(pos,dir,scene.frustrum[i]);
    if(len>0 && (len<ret || ret==0))
      ret = len;
    }
  return ret;
  }

vec3 sunBloom(vec3 refl) {
  const float sunSolidAngle  = 0.53*M_PI/180.0;
  const float minSunCosTheta = cos(sunSolidAngle);

  float cosTheta = dot(refl, scene.sunDir);
  if(cosTheta >= minSunCosTheta)
    return vec3(1.0);
  float offset = minSunCosTheta - cosTheta;
  float gaussianBloom = exp(-offset*50000.0)*0.5;
  float invBloom = 1.0/(0.02 + offset*300.0)*0.01;
  return vec3(gaussianBloom+invBloom);
  }

vec3 ssr(vec3 orig, vec3 start, vec3 refl, const float depth, vec3 sky) {
#if defined(WET)
  const int   SSR_STEPS     = 24;
#else
  const int   SSR_STEPS     = 64;
#endif
  const int   SSR_OCCLUSION = 50;
  const float ZBias         = 0.0;

  const float rayLen = intersectFrustum(start,refl);
  // return vec3(rayLen*0.01);
  if(rayLen<=0)
    return sky;

  const vec4  dest4 = scene.viewProject*vec4(start+refl*rayLen, 1.0);
  const vec3  dest  = dest4.xyz/dest4.w;
  const float off   = interleavedGradientNoise(gl_FragCoord.xy);

  vec2 uv       = orig.xy*0.5+vec2(0.5);
  vec2 uvAlt    = uv;
  bool found    = false;
  int  occluded = 0;

  for(int i=1; i<=SSR_STEPS; ++i) {
    const float t   = (float(i+off)/SSR_STEPS);
    const vec3  pos = mix(orig,dest,t*t);
    if(pos.z>=1.0)
      break;

    const vec2  p        = pos.xy*0.5+vec2(0.5);
    const float smpDepth = textureLod(zbuffer,p,0).r + ZBias;
    if(smpDepth==1.0)
      continue;

    if(smpDepth > pos.z)
      continue; // futher away, than expected

    vec4  hit4   = scene.viewProjectInv*vec4(pos.xy,smpDepth,1.0);
    vec3  hit    = /*normalize*/(hit4.xyz/hit4.w - start);
    float angCos = dot(hit,refl);

    if(angCos<0.0) {
      // screen-space data is occluded
      occluded++;
      uvAlt = p;
      continue;
      }

    uv    = p;
    found = true;
    break;
    }

  // return sky;
  if(!found)
    uv = uvAlt;

  const vec3  reflection = textureLod(sceneColor,uv,0).rgb;
  const float att        = smoothstep(0, 0.1, uv.y);

  if(found)
    return mix(sky,reflection,att);

  if(occluded>SSR_OCCLUSION) {
    const float z  = texelFetch(zbuffer, ivec2(gl_FragCoord.xy), 0).r;

    const vec3 zs = projectiveUnproject(scene.projectInv, vec3(orig.xy, z    ));
    const vec3 zb = projectiveUnproject(scene.projectInv, vec3(orig.xy, depth));

    float h = length(zs - zb)*0.1;
    // float h = 1;
    h = 0.1 + 0.9*smoothstep(0, 0.25, h*0.005);
    return sky*h;
    //return vec3(h);
    }

  return sky;
  }

#if defined(SSR)
vec3 reflection(vec3 orig, vec3 start, vec3 refl, const float depth, vec3 sky) {
  return ssr(orig,start,refl,depth,sky);
  }
#else
vec3 reflection(vec3 orig, vec3 start, vec3 refl, const float depth, vec3 sky) {
  return sky;
  }
#endif

#if defined(WET)
void main() {
  const ivec2 fc        = ivec2(gl_FragCoord.xy);
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);

  // The opaque scene (the depth of the water is not in it). The normals of the G-buffer are cleared by the water pass (only the water has
  // them at this point), so the normal of the ground is made of the derivatives of the position. They are taken before any exit.
  const float depth  = texelFetch(zbuffer, fc, 0).r;
  const vec4  start4 = scene.viewProjectInv*vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  const vec3  start  = start4.xyz/start4.w;
  const vec3  dx     = dFdx(start);
  const vec3  dy     = dFdy(start);

  // the cheap exits first: the sky, the water (it has its own reflection), the far ground
  if(depth>=1.0)
    discard;
  const vec4 diff = texelFetch(gbufDiffuse, fc, 0);
  bool flt, atst, water;
  decodeBits(diff.a, flt, atst, water);
  if(water || scene.underWater!=0)
    discard;

  const float dist = length(start - scene.camPos);
  const float far  = 1.0 - smoothstep(2500.0, 5000.0, dist); // the normal made of the depth is not exact far away
  if(far<=0.0)
    discard;
  // a jump of the depth (an edge, the leaves) has no normal
  if(max(length(dx),length(dy)) > 0.02*dist + 1.0)
    discard;

  vec3 normal = cross(dx,dy);
  if(!(dot(normal,normal) > 1.0e-6))
    discard;
  normal = normalize(normal);
  if(dot(normal, scene.camPos-start) < 0.0)
    normal = -normal;

  const float up = smoothstep(0.6, 0.85, normal.y)*far;
  if(up<=0.0)
    discard;

  // the matrix of the view of the camera gives the position in the world (the Lwc one is relative to the camera): the puddles stay where they are
  const vec3  wpos = start;
  const vec2 covered = wetCovered(wpos);
  if(covered.x>0.5)
    discard;                      // under a roof: dry
  const float rainHere = (covered.y>0.5) ? 0.0 : push.rain; // under the crown of a tree: wet, but no rain falls
  const float pud  = wetPuddle(wpos.xz, push.wet);

  // the wet ground is darker (the puddles more): the scene is multiplied by (1-alpha), the reflection is added
  const float dark = push.wet*(0.25 + 0.25*pud)*up;

  vec3 r = vec3(0);
#if defined(SSR)
  const vec3 view = normalize(start - scene.camPos);
  // the terrain is made of flat triangles: its normal is pulled to the vertical, or the facets of the terrain are seen in the reflection
  vec3 n = normalize(mix(vec3(0.0,1.0,0.0), normal, 0.3));

  // the wet stone: a weak reflection when looking down, a strong one at a low angle (Schlick); the puddles are a mirror
  const float cosT = clamp(dot(-view, n), 0.0, 1.0);
  const float f    = 0.08 + 0.92*pow(1.0-cosT, 4.0);
  const float w    = push.wet*(0.20 + 0.80*pud)*up*f*1.25;

  // the screen space reflection is the expensive part: only where it can be seen
  if(w>0.03) {
    if(pud>0.0 && rainHere>0.0) {
      const vec2 rip = wetRipples(wpos.xz, float(scene.tickCount32%1000000u)*0.001, rainHere)*pud;
      n = normalize(n + vec3(rip.x, 0.0, rip.y)*0.8);
      }

    vec3 refl = reflect(view, n);
    refl.y = max(refl.y, 0.02);
    refl   = normalize(refl);

    vec3 sky = textureSkyLUT(skyLUT, vec3(0,RPlanet,0), refl, scene.sunDir) * scene.GSunIntensity;
    sky *= scene.exposure*push.skyMul;

    r = reflection(vec3(fragCoord.x, fragCoord.y, depth), start, refl, depth, sky) * w;
    // NaN and Inf are not allowed: the blend adds the result to the scene (the ground stays darker, only the reflection is dropped)
    if(!(dot(r,r) < 1.0e12))
      r = vec3(0);
    }
#endif

  outColor = vec4(r, dark);
  }
#else
void main() {
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const vec4  diff      = texelFetch(gbufDiffuse, ivec2(gl_FragCoord.xy), 0);

  if(!isGBufWater(diff.a))
    discard;

  const bool  underWater = (scene.underWater!=0);
  const float ior        = (underWater ? IorAir : IorWater);

  const float depth   = texelFetch (gbufDepth,  ivec2(gl_FragCoord.xy), 0).r;
  const vec3  normal  = normalFetch(gbufNormal, ivec2(gl_FragCoord.xy));
  // a broken normal (NaN) would turn the pixel black: the result is added to the scene
  if(!(dot(normal,normal) > 0.25))
    discard;

  const vec4  start4  = scene.viewProjectInv*vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  const vec3  start   = start4.xyz/start4.w;

  const vec4  scr     = vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  const vec3  camPos  = scene.camPos;

  const vec3  view    = normalize(start - camPos);
        vec3  refl    = reflect(view, normal);
  if(refl.y<0 && !underWater) {
    refl.y = 0;
    refl   = normalize(refl);
    }

  const float f = fresnel(refl,normal,ior);
  if(f<=0.0001)
    discard;

  vec3 sky = vec3(0);
  if(!underWater) {
    vec3 sun = sunBloom(refl) * scene.sunColor;
    sky += textureSkyLUT(skyLUT, vec3(0,RPlanet,0), refl, scene.sunDir) * scene.GSunIntensity;
    sky *= scene.exposure;
    }

  vec3 r = reflection(scr.xyz/scr.w,start,refl,depth,sky) * WaterAlbedo * f;
  // NaN and Inf are not allowed: the blend adds the result to the scene, and a single NaN makes the pixel black
  if(!(dot(r,r) < 1.0e12))
    discard;
  // vec3  r = reflection(scr,start,refl,depth,sky);

  outColor = vec4(r, 0.0);
  //outColor = vec4(sky, 0.0);
  //outColor = vec4(normal, 0.0);
  }
#endif
