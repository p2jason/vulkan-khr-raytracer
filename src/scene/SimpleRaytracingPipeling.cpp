#include "SimpleRaytracingPipeling.h"

#include "Resources.h"

#define ADD_MODULE(l, m) { VkShaderModule shaderModule__ = m; if (shaderModule__ != VK_NULL_HANDLE) return false; (l).push_back(m); }

bool BasicRaytracingPipeline::create(const RaytracingDevice* device, RTPipelineInfo& pipelineInfo)
{
	const RenderDevice* renderDevice = device->getRenderDevice();
	
	VkShaderModule raygenModule = renderDevice->compileShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, Resources::loadShader("shaders/rtsimple/simple.rgen"));
	VkShaderModule missModule = renderDevice->compileShader(VK_SHADER_STAGE_MISS_BIT_KHR, Resources::loadShader("shaders/rtsimple/simple.rmiss"));
	VkShaderModule closestModule = renderDevice->compileShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, Resources::loadShader("shaders/rtsimple/simple.rchit"));

	if (raygenModule == VK_NULL_HANDLE || missModule == VK_NULL_HANDLE || closestModule == VK_NULL_HANDLE)
	{
		return false;
	}

	pipelineInfo.raygenModules.push_back(raygenModule);
	pipelineInfo.missModules.push_back(missModule);
	pipelineInfo.hitGroupModules.push_back({ closestModule, VK_NULL_HANDLE, VK_NULL_HANDLE });

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
	pipelineInfo.descSetLayouts.push_back(m_descSetLayout);

	//Create descriptor pool and allocate descriptor sets
	VkDescriptorPoolSize descPoolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
	};

	VkDescriptorPoolCreateInfo descPoolCI = {};
	descPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolCI.poolSizeCount = sizeof(descPoolSizes) / sizeof(descPoolSizes[0]);
	descPoolCI.pPoolSizes = descPoolSizes;
	descPoolCI.maxSets = 1;
	
	VK_CHECK(vkCreateDescriptorPool(renderDevice->getDevice(), &descPoolCI, nullptr, &m_descriptorPool));

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_descSetLayout;
	
	VK_CHECK(vkAllocateDescriptorSets(renderDevice->getDevice(), &allocInfo, &m_descriptorSet));

	return true;
}

void BasicRaytracingPipeline::createRenderTarget(int width, int height)
{
	m_renderTarget = m_device->getRenderDevice()->createImage2D(width, height, VK_FORMAT_R32G32B32A32_SFLOAT, 1, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	m_renderTargetInitialized = false;

	m_width = width;
	m_height = height;

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

void BasicRaytracingPipeline::bind(VkCommandBuffer commandBuffer)
{
	if (!m_renderTargetInitialized)
	{
		VkImageMemoryBarrier imageBarrier = {};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier.srcAccessMask = 0;
		imageBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.image = m_renderTarget.image;
		imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.layerCount = 1;
		imageBarrier.subresourceRange.baseMipLevel = 0;
		imageBarrier.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
											0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

		m_renderTargetInitialized = true;
	}

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_layout, 0, 1, &m_descriptorSet, 0, nullptr);
}