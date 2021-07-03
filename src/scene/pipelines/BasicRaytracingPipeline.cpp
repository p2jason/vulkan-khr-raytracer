#include "BasicRaytracingPipeline.h"

#include "Common.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct CameraData
{
	glm::mat3 viewMatrix;
	glm::vec3 position;
};

bool BasicRaytracingPipeline::create(const RaytracingDevice* device, RTPipelineInfo& pipelineInfo, std::shared_ptr<Camera> camera, std::shared_ptr<void> reloadOptions)
{
	const RenderDevice* renderDevice = device->getRenderDevice();
	
	//Load pipeline shaders
	std::vector<std::string> definitions = { camera->getCameraDefintions() };

	pipelineInfo.addRaygenShaderFromPath(renderDevice, "asset://shaders/rtsimple/simple.rgen", definitions);
	pipelineInfo.addMissShaderFromPath(renderDevice, "asset://shaders/rtsimple/simple.rmiss", definitions);
	pipelineInfo.addHitGroupFromPath(renderDevice, "asset://shaders/rtsimple/simple.rchit", "asset://shaders/rtsimple/simple.rahit", nullptr,
												   definitions, definitions);

	pipelineInfo.addMissShaderFromPath(renderDevice, "asset://shaders/rtsimple/shadow.rmiss", definitions);

	if (pipelineInfo.failedToLoad)
	{
		return false;
	}

	pipelineInfo.maxRecursionDepth = 2;

	//Create descriptor set layout
	VkDescriptorSetLayoutBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr }
	};

	VkDescriptorSetLayoutCreateInfo descSetLayoutCI = {};
	descSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descSetLayoutCI.bindingCount = sizeof(bindings) / sizeof(bindings[0]);
	descSetLayoutCI.pBindings = bindings;
	
	VK_CHECK(vkCreateDescriptorSetLayout(renderDevice->getDevice(), &descSetLayoutCI, nullptr, &m_descSetLayout));
	pipelineInfo.descSetLayouts.push_back(m_descSetLayout);

	//Create descriptor pool and allocate descriptor sets
	VkDescriptorPoolSize descPoolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
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

	//Create camera data buffer
	VkDescriptorBufferInfo bufferInfo = camera->getDescriptorInfo();

	VkWriteDescriptorSet setWrite = {};
	setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	setWrite.dstSet = m_descriptorSet;
	setWrite.dstBinding = 1;
	setWrite.descriptorCount = 1;
	setWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	setWrite.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(m_device->getRenderDevice()->getDevice(), 1, &setWrite, 0, nullptr);

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
	setWrite.dstBinding = 0;
	setWrite.descriptorCount = 1;
	setWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	setWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(m_device->getRenderDevice()->getDevice(), 1, &setWrite, 0, nullptr);

	notifyCameraChange();
}

void BasicRaytracingPipeline::destroyRenderTarget()
{
	if (m_device == nullptr)
	{
		return;
	}

	m_device->getRenderDevice()->destroyImage(m_renderTarget);
}

const char* BasicRaytracingPipeline::getDescription() const
{
	return "Simple renderer that shows the albedo color of objects, plus shadows";
}

void BasicRaytracingPipeline::drawOptionsUI()
{

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

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_layout, 1, 1, &m_descriptorSet, 0, nullptr);
}