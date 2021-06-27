#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "common/common.glsl"

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT hitPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 2, scalar) buffer VertexNBuffers { vec3 v[]; } normalBuffers[];
layout(set = 0, binding = 3, scalar) buffer VertexTBuffers { vec2 v[]; } texCoordBuffers[];
layout(set = 0, binding = 4, scalar) buffer IndexBuffers { uvec3 i[]; } indexBuffers[];
layout(set = 0, binding = 5) uniform sampler2D albedoTextures[];
layout(set = 0, binding = 6, scalar) buffer MaterialBuffer { Material materialBuffers[]; };

void main() {
	//Pull vertices
	uvec3 indices = indexBuffers[NONUNIFORM_MESH_IDX].i[gl_PrimitiveID];

	vec2 texCoords0 = texCoordBuffers[NONUNIFORM_MESH_IDX].v[indices.x];
	vec2 texCoords1 = texCoordBuffers[NONUNIFORM_MESH_IDX].v[indices.y];
	vec2 texCoords2 = texCoordBuffers[NONUNIFORM_MESH_IDX].v[indices.z];

	float w = 1.0 - attribs.x - attribs.y;

	vec2 texCoords = texCoords0 * w + texCoords1 * attribs.x + texCoords2 * attribs.y;

	//Pull material
	Material material = materialBuffers[gl_InstanceID];

	vec4 color = vec4(1.0, 0.0, 1.0, 1.0);
	
	if (material.albedoIndex != -1) {
		color = texture(albedoTextures[nonuniformEXT(material.albedoIndex)], texCoords);
	}

	vec3 normal0 = normalBuffers[NONUNIFORM_MESH_IDX].v[indices.x];
	vec3 normal1 = normalBuffers[NONUNIFORM_MESH_IDX].v[indices.y];
	vec3 normal2 = normalBuffers[NONUNIFORM_MESH_IDX].v[indices.z];
	
	//Test shadows
	isShadowed = true;
	
	vec3 normal = normalize(normal0 * w + normal1 * attribs.x + normal2 * attribs.y);
	
	vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT + normal * NORMAL_EPSILON;
	const vec3 direction = normalize(vec3(-0.5, 1, 0.2));
	
	uint rayFlags = gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsTerminateOnFirstHitEXT;
	float tMin = NORMAL_EPSILON;
	float tMax = 1000.0;
	
	traceRayEXT(topLevelAS, rayFlags, 0xFF, 0, 0, 1, origin, tMin, direction, tMax, 1);
	
	if (isShadowed)
	{
		color *= 0.3;
	}
	
	payload.hitValue = color.xyz;
}