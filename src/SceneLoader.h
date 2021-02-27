#pragma once

#include <glm/glm.hpp>

#include "api/RaytracingDevice.h"

class SceneRepresentation
{
public:
	TopLevelAS tlas;
	std::vector<BottomLevelAS> blasList;

	const RaytracingDevice* device;
public:
	void destroy();
};

struct MeshVertex
{
	glm::vec3 position;
};

class SceneLoader
{
public:
	static SceneRepresentation loadScene(const RaytracingDevice* device, const char* scenePath);
};