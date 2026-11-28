#ifndef CLOUDS_GLSL
#define CLOUDS_GLSL

vec4 clouds(vec3 at, float nightPhase, vec3 highlight, float overcast, out float density,
            vec2 dxy0, vec2 dxy1,
            in sampler2D dayL1,   in sampler2D dayL0,
            in sampler2D nightL1, in sampler2D nightL0) {
  vec3  cloudsAt = normalize(at);
  vec2  texc     = 2000.0*vec2(atan(cloudsAt.z,cloudsAt.y), atan(cloudsAt.x,cloudsAt.y));

#if defined(SKY_LOD)
  vec4  cloudDL1 = textureLod(dayL1,   texc*0.3 + dxy1, SKY_LOD);
  vec4  cloudDL0 = textureLod(dayL0,   texc*0.3 + dxy0, SKY_LOD);
  vec4  cloudNL1 = textureLod(nightL1, texc*0.3 + dxy1, SKY_LOD);
  vec4  cloudNL0 = textureLod(nightL0, texc*0.6 + vec2(0.5), SKY_LOD); // stars
#else
  vec4  cloudDL1 = texture(dayL1,   texc*0.3 + dxy1);
  vec4  cloudDL0 = texture(dayL0,   texc*0.3 + dxy0);
  vec4  cloudNL1 = texture(nightL1, texc*0.3 + dxy1);
  vec4  cloudNL0 = texture(nightL0, texc*0.6 + vec2(0.5)); // stars
#endif

  // how thick the clouds of the game are here (the textures of both layers): the dark core and the thin edge of the storm
  density      = (cloudDL0.a+cloudDL1.a)*0.5;

  // the rain: the second night layer is SKYRAINCLOUDS.TGA now (Sky::cloudsNight), the dark clouds of the game: where they are thick and thin
  // is the structure of the storm
  const float rainW = smoothstep(0.05, 0.5, overcast);
  density = mix(density, clamp(cloudNL1.a*1.3, 0.0, 1.0), rainW);

  // the rain: the layers get thicker, the gaps between the clouds are covered (the sky is closed by the cloud in the end, in applyClouds)
  const float cloudGain = 0.2 + 1.8*overcast;
  cloudDL0.a   = clamp(cloudDL0.a*cloudGain, 0.0, 1.0);
  cloudDL1.a   = clamp(cloudDL1.a*cloudGain, 0.0, 1.0);
  cloudNL0.a   = cloudNL0.a*1.0; // stars
  cloudNL1.a   = cloudNL1.a*0.1*(1.0-rainW);

  vec4 day       = (cloudDL0+cloudDL1)*0.5;
  vec4 night     = (cloudNL0+cloudNL1)*0.5;

  // Clouds (LDR textures from original game) - need to adjust
  day.rgb   = srgbDecode(day.rgb);
  night.rgb = srgbDecode(night.rgb);

  day.rgb   = day.rgb  *highlight*5.0;
  night.rgb = night.rgb*0.000014;

  day  .a   = day  .a*(1.0-nightPhase);
  //night.a   = night.a*(nightPhase);

  vec4 color = mixClr(day,night);
  // color.rgb += hday;

  return color;
  }

vec3 applyClouds(vec3 skyColor, in sampler2D skyLUT, vec3 plPos, vec3 sunDir, vec3 view, float nightPhase, float overcast,
                 vec2 dxy0, vec2 dxy1,
                 in sampler2D dayL1,   in sampler2D dayL0,
                 in sampler2D nightL1, in sampler2D nightL0) {
  float L = rayIntersect(plPos, view, RClouds);
  // TODO: http://killzone.dl.playstation.net/killzone/horizonzerodawn/presentations/Siggraph15_Schneider_Real-Time_Volumetric_Cloudscapes_of_Horizon_Zero_Dawn.pdf
  // fake cloud scattering inspired by Henyey-Greenstein model
  vec3 lum  = vec3(0);
  lum += textureSkyLUT(skyLUT, plPos, vec3( view.x, view.y*0.0, view.z), sunDir);
  lum += textureSkyLUT(skyLUT, plPos, vec3(-view.x, view.y*0.0, view.z), sunDir);
  lum += textureSkyLUT(skyLUT, plPos, vec3(-view.x, view.y*0.0,-view.z), sunDir);
  lum += textureSkyLUT(skyLUT, plPos, vec3( view.x, view.y*0.0,-view.z), sunDir);
  //return lum;

  float density = 0.0;
  vec4 cloud = clouds((plPos + view*L)*0.01, nightPhase, lum, overcast, density,
                      dxy0, dxy1,
                      dayL1,dayL0, nightL1,nightL0);

  // without the rain: the sum of the original
  vec3 ret = skyColor + cloud.rgb * cloud.a * (1.0-overcast);

  // THE STORM. The clouds of the game (their layers and the places where they are thick) close the whole sky: it is a dark gray cover, a part
  // of the brightness of the sky behind it (the sun is a bright place of it), the thick places are almost black and the thin ones are lighter.
  // The light rain has the thin cover of it, the heavy one has all of it.
  if(overcast>0.0) {
    const float sl     = min(dot(skyColor, vec3(0.2126,0.7152,0.0722)), 1.2);
    const float thick  = smoothstep(0.02, 0.75, density);
    const float shade  = mix(1.35, 0.30, thick);
    const float dark   = mix(0.50, 0.11, smoothstep(0.2, 1.0, overcast));
    const vec3  storm  = vec3(sl) * dark * shade * vec3(0.93,1.0,1.10);

    // the cover: the clouds of the game where they are, the rest of the sky closes when the rain is strong
    const float cover  = mix(clamp(cloud.a*1.6, 0.0, 1.0), 1.0, smoothstep(0.25, 0.8, overcast));
    ret = mix(ret, storm, cover*smoothstep(0.0, 0.5, overcast));
    }
  return ret;
  }
#endif
