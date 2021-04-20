#pragma once

#include "api/RaytracingDevice.h"

#include "scene/SceneLoader.h"

#include <glm/gtc/quaternion.hpp>

#include <filesystem>
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
protected:
	glm::vec3 m_cameraPosition;
	glm::quat m_cameraRotation;

	VkPipelineCache m_cache;
	std::shared_ptr<Scene> m_scene = nullptr;

	std::vector<std::pair<std::string, std::filesystem::file_time_type>> m_monitoredResources;

	const RaytracingDevice* m_device = nullptr;
protected:
	virtual void notifyCameraChange() {}
public:
	virtual bool init(const RaytracingDevice* device, VkPipelineCache cache, std::shared_ptr<Scene> scene) = 0;
	virtual void destroy() = 0;

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
};

class NativeRaytracingPipeline : public RaytracingPipeline
{
private:
	uint32_t m_numRaygenShaders = 0;
	uint32_t m_numMissShaders = 0;
	uint32_t m_numHitGroups = 0;

	uint32_t m_sbtHandleSize = 0;
	uint32_t m_sbtHandleAlignedSize = 0;
	uint32_t m_sbtNumEntries = 0;
protected:
	VkPipeline m_pipeline = VK_NULL_HANDLE;
	VkPipelineLayout m_layout = VK_NULL_HANDLE;

	Buffer m_sbtBuffer;
protected:
	virtual bool create(const RaytracingDevice* device, RTPipelineInfo& pipelineInfo) = 0;
	virtual void clean(const RaytracingDevice* device) {}

	virtual void bind(VkCommandBuffer commandBuffer) {}
public:
	bool init(const RaytracingDevice* device, VkPipelineCache cache, std::shared_ptr<Scene> scene) override;
	void destroy() override;

	void raytrace(VkCommandBuffer buffer);
};