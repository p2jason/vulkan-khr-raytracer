#pragma once

#include "api/RaytracingPipeline.h"

class BasicRaytracingPipeline : public NativeRaytracingPipeline
{
private:
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_descSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

	Buffer m_cameraBuffer;
	void* m_cameraData = nullptr;

	Image m_renderTarget = {};
	int m_width = 1;
	int m_height = 1;

	float m_fov = 70.0f;

	bool m_renderTargetInitialized = false;
private:
	bool create(const RaytracingDevice* device, RTPipelineInfo& pipelineInfo) override;
	void clean(const RaytracingDevice* device) override;

	void bind(VkCommandBuffer commandBuffer) override;

	void notifyCameraChange() override;
public:
	void createRenderTarget(int width, int height);
	void destroyRenderTarget();

	inline Image getRenderTarget() const override { return m_renderTarget; }
	inline glm::ivec2 getRenderTargetSize() const override { return glm::ivec2(m_width, m_height); }
};