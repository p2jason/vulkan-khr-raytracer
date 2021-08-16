#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "common/random.glsl"
#include "common/common.glsl"
#include "sampler_zoo/common.glsl"

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT SamplerZooHitPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 2, scalar) buffer VertexNBuffers { vec3 v[]; } normalBuffers[];
layout(set = 0, binding = 4, scalar) buffer IndexBuffers { uvec3 i[]; } indexBuffers[];

layout(push_constant) uniform PushConstants {
	float lightIntensity;
	int sampleCount;
};

void main() {
	//Pull vertices
	uvec3 indices = indexBuffers[NONUNIFORM_MESH_IDX].i[gl_PrimitiveID];

	vec3 normal0 = normalBuffers[NONUNIFORM_MESH_IDX].v[indices.x];
	vec3 normal1 = normalBuffers[NONUNIFORM_MESH_IDX].v[indices.y];
	vec3 normal2 = normalBuffers[NONUNIFORM_MESH_IDX].v[indices.z];

	//Calculate normals and hit position
	float w = 1.0 - attribs.x - attribs.y;
	
	vec3 normal = normalize(normal0 * w + normal1 * attribs.x + normal2 * attribs.y);
	vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT + normal * NORMAL_EPSILON;
	
	//Compute light coverage
	const vec3 lightPos = vec3(0, 4, 0);
	const float lightRadius = 1.0;
	
	float hitCount = 0;
	
	for (int i = 0; i < sampleCount; ++i) {
#ifdef RNG_USE_HALTON
		vec2 value = 2.0 * halton_rng_generate_2d(payload.rng) - 1.0;
#else
		vec2 value = 2.0 * white_rng_generate_2d(payload.rng) - 1.0;
#endif
		
		vec3 targetPos = lightPos + lightRadius * vec3(value.x, 0, value.y);
		
		//Calculate light direction and distance
		vec3 toLight = targetPos - origin;
		float lightDistance = length(toLight);
		toLight = normalize(toLight);
		
		//Compute shadow hit
		uint rayFlags = gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsTerminateOnFirstHitEXT;
		const float tMin = NORMAL_EPSILON;
		
		isShadowed = true;
		traceRayEXT(topLevelAS, rayFlags, 0xFF, 0, 0, 1, origin, tMin, toLight, lightDistance, 1);
		
		if (!isShadowed) {
			hitCount += max(dot(toLight, normal), 0);
		}
	}
	
	float lightDistance = distance(lightPos, origin);
	
	payload.hitValue = vec3(lightIntensity / (1.0 + lightDistance * lightDistance)) * (float(hitCount) / float(sampleCount));
}