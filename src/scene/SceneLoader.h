#pragma once

#include <glm/glm.hpp>

#include <memory>

#include "api/RaytracingDevice.h"

class Scene
{
public:
	TopLevelAS tlas;
	std::vector<BottomLevelAS> blasList;

	//Descriptor set
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

	const RaytracingDevice* device = nullptr;
public:
	~Scene();
};

struct MeshVertex
{
	glm::vec3 position;
	glm::vec3 normal;
};

class SceneLoader
{
public:
	static std::shared_ptr<Scene> loadScene(const RaytracingDevice* device, const char* scenePath);
};