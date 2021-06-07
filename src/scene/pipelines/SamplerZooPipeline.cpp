#include "SamplerZooPipeline.h"

#include "Common.h"

#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct CameraData
{
	glm::mat3 viewMatrix;
	glm::vec3 position;
};

bool SamplerZooPipeline::create(const RaytracingDevice* device, RTPipelineInfo& pipelineInfo)
{
	const RenderDevice* renderDevice = device->getRenderDevice();

	//Load pipeline shaders
	pipelineInfo.addRaygenShaderFromPath(renderDevice, "asset://shaders/sampler_zoo/sampler_zoo.rgen");
	pipelineInfo.addMissShaderFromPath(renderDevice, "asset://shaders/sampler_zoo/sampler_zoo.rmiss");
	pipelineInfo.addHitGroupFromPath(renderDevice, "asset://shaders/sampler_zoo/sampler_zoo.rchit", "asset://shaders/sampler_zoo/sampler_zoo.rahit", nullptr);

	pipelineInfo.addMissShaderFromPath(renderDevice, "asset://shaders/sampler_zoo/sampler_zoo_shadow.rmiss");

	if (pipelineInfo.failedToLoad)
	{
		return false;
	}

	pipelineInfo.maxRecursionDepth = 2;

	pipelineInfo.pushConstants.push_back({ VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(m_sampleCount) });

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
	m_cameraBuffer = renderDevice->createBuffer(sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	vkMapMemory(renderDevice->getDevice(), m_cameraBuffer.memory, 0, sizeof(CameraData), 0, &m_cameraData);

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = m_cameraBuffer.buffer;
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(CameraData);

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

void SamplerZooPipeline::createRenderTarget(int width, int height)
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

void SamplerZooPipeline::destroyRenderTarget()
{
	if (m_device == nullptr)
	{
		return;
	}

	m_device->getRenderDevice()->destroyImage(m_renderTarget);
}

const char* SamplerZooPipeline::getDescription() const
{
	return "Renderer that demonstrates the difference between sampler types";
}

void SamplerZooPipeline::drawOptionsUI()
{
	ImGui::SetNextItemWidth(150);
	
	if (ImGui::InputInt("Sample count", &m_sampleCount, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		m_sampleCount = std::max(m_sampleCount, 1);
	}
}

void SamplerZooPipeline::notifyCameraChange()
{
	float tanHalfFOV = tan(m_fov / 2.0f);
	float aspectRatio = (float)m_width / m_height;

	glm::mat3 projectionMatrix(glm::vec3(2.0f, 0.0f, 0.0f),
							   glm::vec3(0.0f, 2.0f, 0.0f),
							   glm::vec3(-1.0f, -1.0f, 1.0f));

	projectionMatrix = glm::mat3(glm::vec3(tanHalfFOV * aspectRatio, 0.0f, 0.0f),
								 glm::vec3(0.0f, -tanHalfFOV, 0.0f),
								 glm::vec3(0.0f, 0.0f, 1.0f)) * projectionMatrix;

	projectionMatrix = glm::toMat3(m_cameraRotation) * projectionMatrix;

	((CameraData*)m_cameraData)->viewMatrix = projectionMatrix;
	((CameraData*)m_cameraData)->position = m_cameraPosition;
}

void SamplerZooPipeline::clean(const RaytracingDevice* raytracingDevice)
{
	VkDevice device = raytracingDevice->getRenderDevice()->getDevice();

	vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device, m_descSetLayout, nullptr);

	vkUnmapMemory(device, m_cameraBuffer.memory);
	raytracingDevice->getRenderDevice()->destroyBuffer(m_cameraBuffer);
}

void SamplerZooPipeline::bind(VkCommandBuffer commandBuffer)
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

	vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(m_sampleCount), &m_sampleCount);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_layout, 1, 1, &m_descriptorSet, 0, nullptr);
}