#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "common/common.glsl"

layout(location = 0) rayPayloadInEXT hitPayload payload;

void main() {
	payload.hitValue = vec3(0.0);
}