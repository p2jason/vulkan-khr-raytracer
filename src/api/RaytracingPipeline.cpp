#include "RaytracingPipeline.h"

#include <unordered_set>

#define UINT32_ALIGN(x, a) ((x + (a - 1)) & ~(a - 1))

bool RaytracingPipeline::init(const RaytracingDevice* raytracingDevice, VkPipelineCache cache)
{
	const RenderDevice* renderDevice = raytracingDevice->getRenderDevice();
	VkDevice device = renderDevice->getDevice();

	m_device = raytracingDevice;

	//Get pipeline info
	RTPipelineInfo pipelineInfo;
	if (!create(raytracingDevice, pipelineInfo))
	{
		return false;
	}
	
	//Create pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = (uint32_t)pipelineInfo.descSetLayouts.size();
	pipelineLayoutCI.pSetLayouts = pipelineInfo.descSetLayouts.data();
	pipelineLayoutCI.pushConstantRangeCount = (uint32_t)pipelineInfo.pushConstants.size();
	pipelineLayoutCI.pPushConstantRanges = pipelineInfo.pushConstants.data();

	VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &m_layout));
	
	std::unordered_set<VkShaderModule> modules;
	std::vector<VkPipelineShaderStageCreateInfo> stages;
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;

	//Write raygen shaders
	for (size_t i = 0; i < pipelineInfo.raygenModules.size(); ++i)
	{
		VkRayTracingShaderGroupCreateInfoKHR shaderGroupCI = {};
		shaderGroupCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroupCI.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		shaderGroupCI.generalShader = (uint32_t)stages.size();
		shaderGroupCI.closestHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroupCI.anyHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroupCI.intersectionShader = VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(shaderGroupCI);

		stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, pipelineInfo.raygenModules[i], "main", nullptr });
		modules.insert(stages.back().module);
	}

	m_numRaygenShaders = (uint32_t)shaderGroups.size();

	//Write miss shaders
	for (size_t i = 0; i < pipelineInfo.missModules.size(); ++i)
	{
		VkRayTracingShaderGroupCreateInfoKHR shaderGroupCI = {};
		shaderGroupCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroupCI.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		shaderGroupCI.generalShader = (uint32_t)stages.size();
		shaderGroupCI.closestHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroupCI.anyHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroupCI.intersectionShader = VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(shaderGroupCI);

		stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR, pipelineInfo.missModules[i], "main", nullptr });
		modules.insert(stages.back().module);
	}

	m_numMissShaders = (uint32_t)shaderGroups.size() - m_numRaygenShaders;

	//Write hit groups
	for (size_t i = 0; i < pipelineInfo.hitGroupModules.size(); ++i)
	{
		VkRayTracingShaderGroupCreateInfoKHR shaderGroupCI = {};
		shaderGroupCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroupCI.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		shaderGroupCI.generalShader = VK_SHADER_UNUSED_KHR;
		shaderGroupCI.closestHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroupCI.anyHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroupCI.intersectionShader = VK_SHADER_UNUSED_KHR;
		
		VkShaderModule closestHitModule = std::get<0>(pipelineInfo.hitGroupModules[i]);
		if (closestHitModule != VK_NULL_HANDLE)
		{
			shaderGroupCI.closestHitShader = (uint32_t)stages.size();

			stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closestHitModule, "main", nullptr });
			modules.insert(stages.back().module);
		}

		VkShaderModule anyHitModule = std::get<1>(pipelineInfo.hitGroupModules[i]);
		if (anyHitModule != VK_NULL_HANDLE)
		{
			shaderGroupCI.anyHitShader = (uint32_t)stages.size();

			stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, anyHitModule, "main", nullptr });
			modules.insert(stages.back().module);
		}

		VkShaderModule intersectionModule = std::get<2>(pipelineInfo.hitGroupModules[i]);
		if (intersectionModule != VK_NULL_HANDLE)
		{
			shaderGroupCI.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
			shaderGroupCI.intersectionShader = (uint32_t)stages.size();

			stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, intersectionModule, "main", nullptr });
			modules.insert(stages.back().module);
		}

		shaderGroups.push_back(shaderGroupCI);
	}

	m_numHitGroups = (uint32_t)shaderGroups.size() - m_numMissShaders;

	//Create raytracing pipeline
	VkRayTracingPipelineCreateInfoKHR rtPipelineCI = {};
	rtPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rtPipelineCI.stageCount = (uint32_t)stages.size();
	rtPipelineCI.pStages = stages.data();
	rtPipelineCI.groupCount = (uint32_t)shaderGroups.size();
	rtPipelineCI.pGroups = shaderGroups.data();
	rtPipelineCI.maxPipelineRayRecursionDepth = pipelineInfo.maxRecursionDepth;
	rtPipelineCI.layout = m_layout;

	VK_CHECK(vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, cache, 1, &rtPipelineCI, nullptr, &m_pipeline));

	//Destroy shader modules that are not needed any more
	for (std::unordered_set<VkShaderModule>::iterator it = modules.begin(); it != modules.end(); ++it)
	{
		vkDestroyShaderModule(device, *it, nullptr);
	}

	//Create shader binding table
	m_sbtHandleSize = raytracingDevice->getRTPipelineProperties().shaderGroupHandleSize;
	m_sbtHandleAlignedSize = UINT32_ALIGN(m_sbtHandleSize, raytracingDevice->getRTPipelineProperties().shaderGroupBaseAlignment);
	m_sbtNumEntries = (uint32_t)shaderGroups.size();

	uint32_t sbtSize = m_sbtNumEntries * m_sbtHandleAlignedSize;

	Buffer stagingBuffer = renderDevice->createBuffer(sbtSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	m_sbtBuffer = renderDevice->createBuffer(sbtSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
													  VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//Get shader handles
	uint8_t* shaderHandles = new uint8_t[m_sbtNumEntries * m_sbtHandleSize];
	VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device, m_pipeline, 0, m_sbtNumEntries, m_sbtNumEntries * m_sbtHandleSize, shaderHandles));

	//Write handles to SBT bufer
	uint8_t* memory = nullptr;
	VK_CHECK(vkMapMemory(device, stagingBuffer.memory, 0, sbtSize, 0, (void**)&memory));

	for (uint32_t i = 0; i < shaderGroups.size(); ++i)
	{
		memcpy(memory, &shaderHandles[i * m_sbtHandleSize], m_sbtHandleSize);
		memory += m_sbtHandleAlignedSize;
	}

	vkUnmapMemory(device, stagingBuffer.memory);

	renderDevice->executeCommands(1, [&](VkCommandBuffer* commandBuffers)
	{
		VkBufferCopy region = {};
		region.srcOffset = 0;
		region.dstOffset = 0;
		region.size = sbtSize;

		vkCmdCopyBuffer(commandBuffers[0], stagingBuffer.buffer, m_sbtBuffer.buffer, 1, &region);
	});
	
	//Free staging buffer
	delete[] shaderHandles;

	renderDevice->destroyBuffer(stagingBuffer);

	return true;
}

void RaytracingPipeline::destroy()
{
	clean(m_device);

	VkDevice device = m_device->getRenderDevice()->getDevice();

	m_device->getRenderDevice()->destroyBuffer(m_sbtBuffer);

	if (m_layout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device, m_layout, nullptr);
	}

	if (m_pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, m_pipeline, nullptr);
	}
}

void RaytracingPipeline::raytrace(VkCommandBuffer commandBuffer)
{
	bind(commandBuffer);

	uint32_t width = getRenderTargetSize().x;
	uint32_t height = getRenderTargetSize().y;

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);

	VkDeviceAddress sbtAddress = m_device->getRenderDevice()->getBufferAddress(m_sbtBuffer.buffer);
	VkDeviceSize handleSize = m_sbtHandleAlignedSize;

	VkStridedDeviceAddressRegionKHR addressRegions[] = {
		{
			sbtAddress,
			handleSize,
			m_numRaygenShaders * handleSize
		},
		{
			sbtAddress + m_numRaygenShaders * handleSize,
			handleSize,
			m_numMissShaders * handleSize
		},
		{
			sbtAddress + (m_numRaygenShaders + m_numMissShaders) * handleSize,
			handleSize,
			m_numHitGroups * handleSize
		},
		{ 0, 0, 0 }
	};

	vkCmdTraceRaysKHR(commandBuffer,
					  &addressRegions[0], &addressRegions[1],
					  &addressRegions[2], &addressRegions[3],
					  width, height, 1);
}