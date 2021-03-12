#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "common/random.glsl"
#include "common/common.glsl"
#include "sampler_zoo/common.glsl"

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT SamplerZooHitPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 1, scalar) buffer VertexBuffers { Vertex v[]; } vertexBuffers[];
layout(set = 0, binding = 2) buffer IndexBuffers { uint i[]; } indexBuffers[];
layout(set = 0, binding = 3) uniform sampler2D albedoTextures[];
layout(set = 0, binding = 4, scalar) buffer MaterialBuffer { Material materialBuffers[]; };

void main() {
	//Pull vertices
	uint index0 = indexBuffers[nonuniformEXT(gl_InstanceCustomIndexEXT)].i[3 * gl_PrimitiveID];
	uint index1 = indexBuffers[nonuniformEXT(gl_InstanceCustomIndexEXT)].i[3 * gl_PrimitiveID + 1];
	uint index2 = indexBuffers[nonuniformEXT(gl_InstanceCustomIndexEXT)].i[3 * gl_PrimitiveID + 2];

	Vertex v0 = vertexBuffers[nonuniformEXT(gl_InstanceCustomIndexEXT)].v[index0];
	Vertex v1 = vertexBuffers[nonuniformEXT(gl_InstanceCustomIndexEXT)].v[index1];
	Vertex v2 = vertexBuffers[nonuniformEXT(gl_InstanceCustomIndexEXT)].v[index2];

	float w = 1.0 - attribs.x - attribs.y;

	vec3 normal = normalize(v0.normal * w + v1.normal * attribs.x + v2.normal * attribs.y);
	vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT + normal * 0.001;
	
	const vec3 lightPos = vec3(0, 3, 0);
	const float lightRadius = 1.0;
	
	//Compute shader coverage
	const int numSamples = 32;
	int hitCount = 0;
	
	for (int i = 0; i < numSamples; ++i) {
		vec2 value = white_rng_generate_2d(payload.rng);
		
		vec3 targetPos = lightPos + lightRadius * normalizedToUnitSphere(value.x, value.y);
		
		//Calculate light direction and distance
		vec3 toLight = targetPos - origin;
		float lightDistance = length(toLight);
		toLight = normalize(toLight);
		
		//Compute shadow hit
		uint rayFlags = gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsTerminateOnFirstHitEXT;
		const float tMin = 0.001;
		
		isShadowed = true;
		traceRayEXT(topLevelAS, rayFlags, 0xFF, 0, 0, 1, origin, tMin, toLight, lightDistance, 1);
		
		if (!isShadowed) {
			hitCount++;
		}
	}
	
	float lightDistance = distance(lightPos, origin);
	
	payload.hitValue = vec3(10.0 / (1.0 + lightDistance * lightDistance)) * (float(hitCount) / float(numSamples));
}