#ifndef SAMPLER_ZOO_COMMON_GLSL
#define SAMPLER_ZOO_COMMON_GLSL

#include "common/random.glsl"

struct SamplerZooHitPayload {
	vec3 hitValue;
	HaltonRngState rng;
};

#endif
