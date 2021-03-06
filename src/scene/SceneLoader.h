#pragma once

#include <glm/glm.hpp>

#include <memory>

#include "api/RaytracingDevice.h"

struct Material
{
	uint32_t albedoIndex;
};

struct MeshBuffers
{
	Buffer vertexBuffer;
	VkDeviceSize vertexOffset;
	VkDeviceSize vertexSize;

	Buffer indexBuffer;
	VkDeviceSize indexOffset;
	VkDeviceSize indexSize;
};

class Scene
{
public:
	TopLevelAS tlas;
	std::vector<BottomLevelAS> blasList;
	std::vector<MeshBuffers> meshBuffers;

	std::vector<std::pair<Image, VkSampler>> textures;
	std::vector<Material> materials;

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
	glm::vec2 texCoords;
};

class SceneLoader
{
public:
	static std::shared_ptr<Scene> loadScene(const RaytracingDevice* device, const char* scenePath);
};