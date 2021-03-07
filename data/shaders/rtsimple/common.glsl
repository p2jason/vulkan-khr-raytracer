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