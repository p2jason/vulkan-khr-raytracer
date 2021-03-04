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

void main() {
	uint index0 = indexBuffers[nonuniformEXT(gl_InstanceCustomIndexEXT)].i[3 * gl_PrimitiveID];
	uint index1 = indexBuffers[nonuniformEXT(gl_InstanceCustomIndexEXT)].i[3 * gl_PrimitiveID + 1];
	uint index2 = indexBuffers[nonuniformEXT(gl_InstanceCustomIndexEXT)].i[3 * gl_PrimitiveID + 2];

	Vertex v0 = vertexBuffers[nonuniformEXT(gl_InstanceCustomIndexEXT)].v[index0];
	Vertex v1 = vertexBuffers[nonuniformEXT(gl_InstanceCustomIndexEXT)].v[index1];
	Vertex v2 = vertexBuffers[nonuniformEXT(gl_InstanceCustomIndexEXT)].v[index2];

	float w = 1.0 - attribs.x - attribs.y;

	vec3 color = v0.normal * w + v1.normal * attribs.x + v2.normal * attribs.y;

	payload.hitValue = 0.5 * color + 0.5;
}