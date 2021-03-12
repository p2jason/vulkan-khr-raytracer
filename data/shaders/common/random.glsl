#ifndef RANDOM_GLSL
#define RANDOM_GLSL

const int INT_MAX = 0x7FFFFFFF;
const int INT_MIN = 0x80000000;

struct WhiteRngState {
    int seed1;
	int seed2;
};

int xorshift(int seed) {
    // Xorshift*32
    // Based on George Marsaglia's work: http://www.jstatsoft.org/v08/i14/paper
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    
    return seed;
}

void white_rng_init(inout WhiteRngState state, vec2 uv) {
    float seed1 = fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453123);
	state.seed1 = int(seed1 * float(INT_MAX));
	
	float seed2 = fract(sin(dot(uv, vec2(53.9183, 31.564))) * 32854.5846487);
	state.seed2 = int(seed2 * float(INT_MAX));
}

float white_rng_generate_1d(inout WhiteRngState state) {
    int value = xorshift(state.seed1++);
    
    //Convert to float
    return fract(abs(fract(float(value) / 4271.592653)));
}

vec2 white_rng_generate_2d(inout WhiteRngState state) {
    float value1 = float(xorshift(state.seed1++));
	float value2 = float(xorshift(state.seed2++));
    
    //Convert to float
    return vec2(fract(abs(fract(value1 / 4271.592653))),
				fract(abs(fract(value2 / 3983.347946))));
}

#endif
