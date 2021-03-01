#include "SimpleRaytracingPipeling.h"

const char* RAYGEN_SHADER = R"(#version 460

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 1, rgb32f) uniform accelerationStructureEXT topLevelAS;

void main() {
	imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(0.1, 0.7, 0.55, 1.0));
})";

const char* MISS_SHADER = R"(#version 430

void main() {

})";

const char* CLOSEST_HIT_SHADER = R"(#version 430

void main() {

})";

RTPipelineInfo BasicRaytracingPipeline::create(const RaytracingDevice* device)
{
	RTPipelineInfo pipelineInfo = {};

	const RenderDevice* renderDevice = device->getRenderDevice();

	//Compile raygen shader
	VkShaderModule raygenModule = renderDevice->compileShader(device->getRenderDevice(), VK_SHADER_STAGE_RAYGEN_BIT_KHR, RAYGEN_SHADER);
	pipelineInfo.stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygenModule, "main", nullptr });

	VkRayTracingShaderGroupCreateInfoKHR shaderGroupCI = {};
	shaderGroupCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroupCI.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroupCI.generalShader = (uint32_t)pipelineInfo.stages.size() - 1;
	shaderGroupCI.closestHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroupCI.anyHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroupCI.intersectionShader = VK_SHADER_UNUSED_KHR;
	pipelineInfo.rtShaderGroups.push_back(shaderGroupCI);

	//Compile miss shader
	VkShaderModule missModule = renderDevice->compileShader(device->getRenderDevice(), VK_SHADER_STAGE_MISS_BIT_KHR, MISS_SHADER);
	pipelineInfo.stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR, missModule, "main", nullptr });

	VkRayTracingShaderGroupCreateInfoKHR shaderGroupCI = {};
	shaderGroupCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroupCI.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroupCI.generalShader = (uint32_t)pipelineInfo.stages.size() - 1;
	shaderGroupCI.closestHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroupCI.anyHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroupCI.intersectionShader = VK_SHADER_UNUSED_KHR;
	pipelineInfo.rtShaderGroups.push_back(shaderGroupCI);

	//Compile closest hit shader
	VkShaderModule closestHitModule = renderDevice->compileShader(device->getRenderDevice(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, CLOSEST_HIT_SHADER);
	pipelineInfo.stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closestHitModule, "main", nullptr });

	VkRayTracingShaderGroupCreateInfoKHR shaderGroupCI = {};
	shaderGroupCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroupCI.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroupCI.generalShader = VK_SHADER_UNUSED_KHR;
	shaderGroupCI.closestHitShader = (uint32_t)pipelineInfo.stages.size() - 1;
	shaderGroupCI.anyHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroupCI.intersectionShader = VK_SHADER_UNUSED_KHR;
	pipelineInfo.rtShaderGroups.push_back(shaderGroupCI);

	pipelineInfo.cleanupModules.push_back(raygenModule);
	pipelineInfo.cleanupModules.push_back(missModule);
	pipelineInfo.cleanupModules.push_back(closestHitModule);

	//Create descriptor set layout
	VkDescriptorSetLayoutBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr }
	};

	VkDescriptorSetLayoutCreateInfo descSetLayoutCI = {};
	descSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descSetLayoutCI.bindingCount = sizeof(bindings) / sizeof(bindings[0]);
	descSetLayoutCI.pBindings = bindings;
	
	VK_CHECK(vkCreateDescriptorSetLayout(renderDevice->getDevice(), &descSetLayoutCI, nullptr, &m_descSetLayout));

	//Create descriptor pool and allocate descriptor sets
	VkDescriptorPoolSize descPoolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
	};

	VkDescriptorPoolCreateInfo descPoolCI = {};
	descPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolCI.poolSizeCount = sizeof(descPoolSizes) / sizeof(descPoolSizes[0]);
	descPoolCI.pPoolSizes = descPoolSizes;
	
	VK_CHECK(vkCreateDescriptorPool(renderDevice->getDevice(), &descPoolCI, nullptr, &m_descriptorPool));

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_descSetLayout;
	
	VK_CHECK(vkAllocateDescriptorSets(renderDevice->getDevice(), &allocInfo, &m_descriptorSet));

	return pipelineInfo;
}

void BasicRaytracingPipeline::createRenderTarget(int width, int height)
{
	m_renderTarget = m_device->getRenderDevice()->createImage2D(width, height, VK_FORMAT_R32G32B32_SFLOAT, 1, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = m_renderTarget.imageView;

	VkWriteDescriptorSet setWrite = {};
	setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	setWrite.dstSet = m_descriptorSet;
	setWrite.dstBinding = 1;
	setWrite.descriptorCount = 1;
	setWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	setWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(m_device->getRenderDevice()->getDevice(), 1, &setWrite, 0, nullptr);
}

void BasicRaytracingPipeline::destroyRenderTarget()
{
	m_device->getRenderDevice()->destroyImage(m_renderTarget);
}

void BasicRaytracingPipeline::bindToScene(const SceneRepresentation& scene) const
{
	//Write TLAS
	VkAccelerationStructureKHR tlas = scene.tlas.get();

	VkWriteDescriptorSetAccelerationStructureKHR tlasSetWrite = {};
	tlasSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	tlasSetWrite.accelerationStructureCount = 1;
	tlasSetWrite.pAccelerationStructures = &tlas;

	//Write other
	VkWriteDescriptorSet setWrites[1] = {};
	setWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	setWrites[0].pNext = &tlasSetWrite;
	setWrites[0].dstSet = m_descriptorSet;
	setWrites[0].dstBinding = 0;
	setWrites[0].descriptorCount = 1;
	setWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	vkUpdateDescriptorSets(m_device->getRenderDevice()->getDevice(), sizeof(setWrites) / sizeof(setWrites[0]), setWrites, 0, nullptr);
}

void BasicRaytracingPipeline::clean(const RaytracingDevice* raytracingDevice)
{
	VkDevice device = raytracingDevice->getRenderDevice()->getDevice();

	vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device, m_descSetLayout, nullptr);
}