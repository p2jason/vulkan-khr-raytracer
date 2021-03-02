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
	int m_width = 0;
	int m_height = 0;

	bool m_renderTargetInitialized = false;
private:
	bool create(const RaytracingDevice* device, RTPipelineInfo& pipelineInfo) override;
	void clean(const RaytracingDevice* device) override;

	void bind(VkCommandBuffer commandBuffer) override;
public:
	void createRenderTarget(int width, int height);
	void destroyRenderTarget();

	void bindToScene(const SceneRepresentation& scene) const;

	inline Image getRenderTarget() const override { return m_renderTarget; }
	inline glm::ivec2 getRenderTargetSize() const override { return glm::ivec2(m_width, m_height); }
};