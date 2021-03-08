#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "rtsimple/common.glsl"

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT hitPayload payload;
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

	vec2 texCoords = v0.texCoords * w + v1.texCoords * attribs.x + v2.texCoords * attribs.y;

	//Pull material
	Material material = materialBuffers[gl_InstanceID];

	vec4 color = vec4(1.0, 0.0, 1.0, 1.0);
	
	if (material.albedoIndex != -1) {
		color = texture(albedoTextures[nonuniformEXT(material.albedoIndex)], texCoords);
	}
	
	//Test shadows
	isShadowed = true;
	
	vec3 normal = v0.normal * w + v1.normal * attribs.x + v2.normal * attribs.y;
	
    vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT + normal * 0.001;
	const vec3 direction = normalize(vec3(-0.5, 1, 0.03));
	
	uint rayFlags = gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsTerminateOnFirstHitEXT;
	float tMin = 0.01;
	float tMax = 10000.0;
	
	traceRayEXT(topLevelAS, rayFlags, 0xFF, 0, 0, 1, origin, tMin, direction, tMax, 1);
	
	if (isShadowed)
	{
		color *= 0.3;
	}
	
	payload.hitValue = color.xyz;
}