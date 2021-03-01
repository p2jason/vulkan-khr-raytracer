#include "RaytracingPipeline.h"

bool RaytracingPipeline::init(const RaytracingDevice* raytracingDevice, VkPipelineCache cache)
{
	VkDevice device = raytracingDevice->getRenderDevice()->getDevice();
	m_device = raytracingDevice;

	//Get pipeline info
	RTPipelineInfo pipelineInfo = create(raytracingDevice);

	//Create pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = (uint32_t)pipelineInfo.descSetLayouts.size();
	pipelineLayoutCI.pSetLayouts = pipelineInfo.descSetLayouts.data();
	pipelineLayoutCI.pushConstantRangeCount = (uint32_t)pipelineInfo.pushConstants.size();
	pipelineLayoutCI.pPushConstantRanges = pipelineInfo.pushConstants.data();

	VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &m_layout));

	//Create raytracing pipeline
	VkRayTracingPipelineCreateInfoKHR rtPipelineCI = {};
	rtPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rtPipelineCI.stageCount = (uint32_t)pipelineInfo.stages.size();
	rtPipelineCI.pStages = pipelineInfo.stages.data();
	rtPipelineCI.groupCount = (uint32_t)pipelineInfo.rtShaderGroups.size();
	rtPipelineCI.pGroups = pipelineInfo.rtShaderGroups.data();
	rtPipelineCI.maxPipelineRayRecursionDepth = pipelineInfo.maxRecursionDepth;
	rtPipelineCI.layout = m_layout;

	VK_CHECK(vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, cache, 1, &rtPipelineCI, nullptr, &m_pipeline));

	//Destroy shader modules that are not needed any more
	for (size_t i = 0; i < pipelineInfo.cleanupModules.size(); ++i)
	{
		vkDestroyShaderModule(device, pipelineInfo.cleanupModules[i], nullptr);
	}
}

void RaytracingPipeline::destroy()
{
	clean(m_device);

	VkDevice device = m_device->getRenderDevice()->getDevice();

	if (m_layout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device, m_layout, nullptr);
	}

	if (m_pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, m_pipeline, nullptr);
	}
}