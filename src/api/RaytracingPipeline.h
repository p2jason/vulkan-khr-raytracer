#pragma once

#include "api/RaytracingDevice.h"

#include "scene/SceneLoader.h"

#include <glm/gtc/quaternion.hpp>

#include <string>

struct RTPipelineInfo
{
	std::vector<VkShaderModule> raygenModules;
	std::vector<VkShaderModule> missModules;
	std::vector<std::tuple<VkShaderModule, VkShaderModule, VkShaderModule>> hitGroupModules;

	std::vector<VkDescriptorSetLayout> descSetLayouts;
	std::vector<VkPushConstantRange> pushConstants;

	int maxRecursionDepth = 1;
};

class RaytracingPipeline
{
private:
	uint32_t m_numRaygenShaders = 0;
	uint32_t m_numMissShaders = 0;
	uint32_t m_numHitGroups = 0;

	uint32_t m_sbtHandleSize = 0;
	uint32_t m_sbtHandleAlignedSize = 0;
	uint32_t m_sbtNumEntries = 0; 

	std::shared_ptr<Scene> m_scene = nullptr;
protected:
	VkPipeline m_pipeline = VK_NULL_HANDLE;
	VkPipelineLayout m_layout = VK_NULL_HANDLE;

	Buffer m_sbtBuffer;

	glm::vec3 m_cameraPosition;
	glm::quat m_cameraRotation;

	const RaytracingDevice* m_device = nullptr;
protected:
	virtual bool create(const RaytracingDevice* device, RTPipelineInfo& pipelineInfo) = 0;
	virtual void clean(const RaytracingDevice* device) {}

	virtual void bind(VkCommandBuffer commandBuffer) {}

	virtual void notifyCameraChange() {}
public:
	bool init(const RaytracingDevice* device, VkPipelineCache cache, std::shared_ptr<Scene> scene);
	void destroy();

	void raytrace(VkCommandBuffer buffer);

	virtual Image getRenderTarget() const = 0;
	virtual glm::ivec2 getRenderTargetSize() const = 0;

	inline glm::vec3 getCameraPosition() const { return m_cameraPosition; }
	inline glm::quat getCameraRotation() const { return m_cameraRotation; }

	inline void setCameraPosition(glm::vec3 position) { m_cameraPosition = position; notifyCameraChange(); }
	inline void setCameraRotation(glm::quat rotation) { m_cameraRotation = rotation; notifyCameraChange(); }

	inline void setCameraData(glm::vec3 position, glm::quat rotation)
	{
		m_cameraPosition = position;
		m_cameraRotation = rotation;

		notifyCameraChange();
	}

	inline VkPipeline getPipeline() const { return m_pipeline; }
};