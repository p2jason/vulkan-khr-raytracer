#version 450

layout(location = 0) out vec2 texCoord;

layout(set = 0, binding = 1) uniform Constants {
	int windowWidth;
	int windowHeight;
	
	int quadWidth;
	int quadHeight;
};

vec2 vertices[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(-1.0, 1.0),

	vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0)
);

void main() {
	texCoord = 0.5 * vertices[gl_VertexIndex] + 0.5;
	
	float resizeRatio = min(float(windowWidth) / quadWidth, float(windowHeight) / quadHeight);
	
	vec2 vertex = vertices[gl_VertexIndex];
	vertex *= vec2(quadWidth, quadHeight) / vec2(windowWidth, windowHeight);
	vertex *= resizeRatio;
	
	gl_Position = vec4(vertex, 0.0, 1.0);
}