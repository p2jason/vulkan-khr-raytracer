#pragma once

#include "api/RaytracingDevice.h"

#include <string>

struct RTPipelineInfo
{
	std::vector<VkPipelineShaderStageCreateInfo> stages;
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> rtShaderGroups;

	std::vector<VkDescriptorSetLayout> descSetLayouts;
	std::vector<VkPushConstantRange> pushConstants;

	int maxRecursionDepth = 1;

	std::vector<VkShaderModule> cleanupModules;
};

class RaytracingPipeline
{
protected:
	VkPipeline m_pipeline = VK_NULL_HANDLE;
	VkPipelineLayout m_layout = VK_NULL_HANDLE;

	RTPipelineInfo m_pipelineInfo = {};

	const RaytracingDevice* m_device = nullptr;
protected:
	virtual RTPipelineInfo create(const RaytracingDevice* device) = 0;
	virtual void clean(const RaytracingDevice* device) {}
public:
	bool init(const RaytracingDevice* device, VkPipelineCache cache);
	void destroy();

	inline const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& getShaderGroups() const { return m_pipelineInfo.rtShaderGroups; }

	inline VkPipeline getPipeline() const { return m_pipeline; }
	inline VkPipelineLayout getPipelineLayout() const { return m_layout; }
};