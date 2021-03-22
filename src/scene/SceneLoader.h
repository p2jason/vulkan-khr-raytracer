#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>

#include "api/RaytracingDevice.h"

struct Material
{
	uint32_t albedoIndex;
};

struct MeshBuffers
{
	Buffer vertexBuffer;
	std::pair<VkDeviceSize, VkDeviceSize> positionRange;
	std::pair<VkDeviceSize, VkDeviceSize> texCoordRange;
	std::pair<VkDeviceSize, VkDeviceSize> normalRange;

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
	std::vector<bool> isMaterialOpaque;
	Buffer materialBuffer;

	glm::vec3 cameraPosition;
	glm::quat cameraRotation;

	//Descriptor set
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

	const RaytracingDevice* device = nullptr;
public:
	~Scene();
};

class SceneLoader
{
public:
	static std::shared_ptr<Scene> loadScene(const RaytracingDevice* device, const char* scenePath);
};