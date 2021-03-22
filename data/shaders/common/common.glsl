#ifndef COMMON_GLSL
#define COMMON_GLSL

#define MATH_PI (3.141592653589793)
#define MATH_PI_HALF (0.5 * 3.141592653589793)
#define MATH_PI_DOUBLE (2.0 * 3.141592653589793)

#define NONUNIFORM_MESH_IDX (nonuniformEXT(gl_InstanceCustomIndexEXT))

struct Vertex
{
	//TODO: Split Vertex into SoA instead of AoS to allows for accessing only specific elements
	//of the vertex, like vertices or texture coordinates.
	
	vec3 position;
	vec3 normal;
	vec2 texCoords;
};

struct hitPayload
{
	vec3 hitValue;
};

struct Material
{
	//TODO: SoA (same as vertex)
	uint albedoIndex;
};

//theta - angle aroung up axis (0, 2pi)
//phi - angle between horizontal plane and the point (-pi/2, pi/2)
vec3 toUnitSphere(float theta, float phi) {
	float cosPhi = cos(phi);

	return vec3(cos(theta) * cosPhi,
				sin(phi),
				sin(theta) * cosPhi);
}

vec3 normalizedToUnitSphere(float theta, float phi) {
	return toUnitSphere(MATH_PI_DOUBLE * theta, MATH_PI * phi - MATH_PI_HALF);
}

#endif
