#ifndef SAMPLER_ZOO_COMMON_GLSL
#define SAMPLER_ZOO_COMMON_GLSL

#include "common/random.glsl"

struct SamplerZooHitPayload {
	vec3 hitValue;
#ifdef RNG_USE_HALTON
	HaltonRngState rng;
#else
	WhiteRngState rng;
#endif	
};

#endif
