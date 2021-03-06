#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "rtsimple/common.glsl"

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT hitPayload payload;

layout(set = 0, binding = 1, scalar) buffer VertexBuffers { Vertex v[]; } vertexBuffers[];
layout(set = 0, binding = 2) buffer IndexBuffers { uint i[]; } indexBuffers[];
layout(set = 0, binding = 3) uniform sampler2D albedoTextures[];

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
	vec4 color = texture(albedoTextures[1], texCoords);

	payload.hitValue = color.xyz;
}