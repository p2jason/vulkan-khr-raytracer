#pragma once

#include "api/RaytracingPipeline.h"

class SamplerZooPipeline : public NativeRaytracingPipeline
{
private:
	struct PushConstants
	{
		int sampleCount = 16;

		float lightIntensity = 7.0f;
		float lightRadius = 1.0;
		glm::vec3 lightPos = { 0, 4, 0 };
	};

	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_descSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

	Image m_renderTarget = {};
	int m_width = 1;
	int m_height = 1;

	PushConstants m_pushConstants = {};
	int m_samplerIndex = 0;

	bool m_renderTargetInitialized = false;
private:
	bool create(const RaytracingDevice* device, RTPipelineInfo& pipelineInfo, std::shared_ptr<Camera> camera, std::shared_ptr<void> reloadOptions) override;
	void clean(const RaytracingDevice* device) override;

	void bind(VkCommandBuffer commandBuffer) override;
public:
	void createRenderTarget(int width, int height) override;
	void destroyRenderTarget() override;

	inline std::string getDefaultScene() const override { return "asset://scenes/shadow_test.glb"; }

	const char* getDescription() const override;
	void drawOptionsUI() override;

	inline Image getRenderTarget() const override { return m_renderTarget; }
	inline glm::ivec2 getRenderTargetSize() const override { return glm::ivec2(m_width, m_height); }

	std::shared_ptr<void> getReloadOptions() const override { return std::make_shared<std::pair<PushConstants, int>>(m_pushConstants, m_samplerIndex); }
};