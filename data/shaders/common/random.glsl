#ifndef RANDOM_GLSL
#define RANDOM_GLSL

const int INT_MAX = 0x7FFFFFFF;
const int INT_MIN = 0x80000000;

const float FLOAT_NAN = sqrt(-1);

int xorshift(int seed) {
    // Xorshift*32
    // Based on George Marsaglia's work: http://www.jstatsoft.org/v08/i14/paper
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    
    return seed;
}

/*
White noise RNG
===============

Generates "vanilla" random numbers. This is the same type of randomness you
would get from RNG classes in most programming languages.
*/

struct WhiteRngState {
    int seed1;
	int seed2;
};

void white_rng_init(inout WhiteRngState state, vec2 uv, int seed) {
    float seed1 = fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453123);
	state.seed1 = int(seed1 * float(INT_MAX)) + seed;
	
	float seed2 = fract(sin(dot(uv, vec2(53.9183, 31.564))) * 32854.5846487);
	state.seed2 = int(seed2 * float(INT_MAX)) + seed;
}

float white_rng_generate_1d(inout WhiteRngState state) {
    float value = float(xorshift(state.seed1++));
    
    //Convert to float
	const float divisor = 1.0 / 4271.592653;
	
    return abs(fract(value * divisor));
}

vec2 white_rng_generate_2d(inout WhiteRngState state) {
    float value1 = float(xorshift(state.seed1++));
	float value2 = float(xorshift(state.seed2++));
    
    //Convert to float
	const float divisor1 = 1.0 / 4271.592653;
	const float divisor2 = 1.0 / 3983.347946;
	
    return vec2(abs(fract(value1 * divisor1)),
				abs(fract(value2 * divisor2)));
}

/*
Halton RNG
==============
*/

struct HaltonRngState {
	uint idx;
	uint base;
};

void halton_rng_init(inout HaltonRngState state, int base1, int base2, vec2 uv, int seed) {
	float seed1 = fract(sin(dot(uv, vec2(53.9183, 31.564))) * 32854.5846487);
	state.idx = uint(xorshift(int(seed1 * float(INT_MAX)) + seed));
	
	state.base = ((base2 & 0xFFFF) << 16) | (base1 & 0xFFFF);
}

float halton_rng_generate(inout HaltonRngState state, bool dimTwo) {
	float fraction = 1.0;
	float result = 0;
	
	//Extract base
	uint base = 0;
	
	if (dimTwo) {
		base = state.base >> 16;
	} else {
		base = state.base;
	}
	
	base &= 0xFFFF;
	
	//Calculate halton value
	float invBase = 1.0 / base;
	
	uint idx = state.idx;
	while (idx  > 0) {
		fraction *= invBase;
		result += fraction * (idx % base);
		idx = ~~(idx / base);
	}
	
	return result;
}

float halton_rng_generate_1d(inout HaltonRngState state) {
	float value = halton_rng_generate(state, false);
	state.idx++;
	
	return value;
}

vec2 halton_rng_generate_2d(inout HaltonRngState state) {
	vec2 value = vec2(halton_rng_generate(state, false),
					  halton_rng_generate(state, true));
	state.idx++;
	
	return value;
}

#endif
