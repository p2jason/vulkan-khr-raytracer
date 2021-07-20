#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>
#include <mutex>
#include <atomic>

#include "api/RaytracingDevice.h"

struct Material
{
	uint32_t albedoIndex;
};

struct MeshBuffers
{
	VkBuffer vertexBuffer;
	std::pair<VkDeviceSize, VkDeviceSize> positionRange;
	std::pair<VkDeviceSize, VkDeviceSize> texCoordRange;
	std::pair<VkDeviceSize, VkDeviceSize> normalRange;

	VkBuffer indexBuffer;
	VkDeviceSize indexOffset;
	VkDeviceSize indexSize;
};

class SceneLoadProgress
{
public:
	std::mutex lock;

	int progressStage;
	int numStages;

	std::string stageDescription;
	float stageProgess;
public:
	SceneLoadProgress() : progressStage(0), numStages(1), stageProgess(0.0f) {}

	void begin(int stageCount, std::string firstDesc)
	{
		std::lock_guard<std::mutex> guard(lock);

		numStages = stageCount;
		stageDescription = firstDesc;
	}

	void setStageProgress(float p)
	{
		std::lock_guard<std::mutex> guard(lock);

		stageProgess = p;
	}

	void nextStage(std::string desc)
	{
		std::lock_guard<std::mutex> guard(lock);

		stageDescription = desc;
		progressStage++;
		stageProgess = 0.0f;
	}

	void finish()
	{
		std::lock_guard<std::mutex> guard(lock);

		progressStage = numStages;
		stageProgess = 1.0f;
	}
};

class Scene
{
public:
	TopLevelAS tlas;
	std::vector<BottomLevelAS> blasList;

	VkDeviceMemory sceneMemory;
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
	static std::shared_ptr<Scene> loadScene(const RaytracingDevice* device, const char* scenePath, std::shared_ptr<SceneLoadProgress> progress = nullptr);
};