#ifndef CAMERA_PERSPECTIVE_GLSL
#define CAMERA_PERSPECTIVE_GLSL

#extension GL_EXT_scalar_block_layout : enable

layout(set = 1, binding = 1, scalar) uniform CameraData {
	mat3 cameraMatrix;
	vec3 cameraPosition;
};

void calcCameraRay(vec2 pixelPos, vec2 screenSize, out vec3 rayPos, out vec3 rayDir) {
	vec2 pixelUV = (pixelPos + vec2(0.5)) / screenSize;

	rayDir = normalize(cameraMatrix * vec3(pixelUV, 1.0));
	rayPos = cameraPosition;
}

#endif
