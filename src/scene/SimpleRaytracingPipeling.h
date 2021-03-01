#pragma once

#include "api/RaytracingPipeline.h"
#include "SceneLoader.h"

class BasicRaytracingPipeline : public RaytracingPipeline
{
private:
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_descSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

	Image m_renderTarget = {};
public:
	RTPipelineInfo create(const RaytracingDevice* device) override;
	void clean(const RaytracingDevice* device) override;

	void createRenderTarget(int width, int height);
	void destroyRenderTarget();

	void bindToScene(const SceneRepresentation& scene) const;
};