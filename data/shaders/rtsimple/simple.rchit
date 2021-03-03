#version 460
#extension GL_EXT_ray_tracing : require

struct hitPayload
{
	vec3 hitValue;
};

layout(location = 0) rayPayloadInEXT hitPayload payload;

void main() {
	payload.hitValue = vec3(1.0, 0.0, 0.0);
}