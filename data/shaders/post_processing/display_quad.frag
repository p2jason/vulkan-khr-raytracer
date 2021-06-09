#version 450

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D renderTarget;

void main() {
	vec3 color = texture(renderTarget, texCoord).rgb;

	outColor = vec4(color, 1.0);
}