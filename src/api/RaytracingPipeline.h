#pragma once

#include "api/RaytracingDevice.h"

#include "scene/SceneLoader.h"

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

	const RaytracingDevice* m_device = nullptr;
protected:
	virtual bool create(const RaytracingDevice* device, RTPipelineInfo& pipelineInfo) = 0;
	virtual void clean(const RaytracingDevice* device) {}

	virtual void bind(VkCommandBuffer commandBuffer) {}
public:
	bool init(const RaytracingDevice* device, VkPipelineCache cache, std::shared_ptr<Scene> scene);
	void destroy();

	void raytrace(VkCommandBuffer buffer);

	virtual Image getRenderTarget() const = 0;
	virtual glm::ivec2 getRenderTargetSize() const = 0;

	inline VkPipeline getPipeline() const { return m_pipeline; }
};