#pragma once

#include "api/RaytracingPipeline.h"

class BasicRaytracingPipeline : public NativeRaytracingPipeline
{
private:
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_descSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

	Image m_renderTarget = {};
	int m_width = 1;
	int m_height = 1;

	bool m_renderTargetInitialized = false;
private:
	bool create(const RaytracingDevice* device, RTPipelineInfo& pipelineInfo, std::shared_ptr<Camera> camera, std::shared_ptr<void> reloadOptions) override;
	void clean(const RaytracingDevice* device) override;

	void bind(VkCommandBuffer commandBuffer) override;
public:
	void createRenderTarget(int width, int height);
	void destroyRenderTarget();

	inline std::string getDefaultScene() const override { return "C:\\Users\\Jason\\Downloads\\sponza\\sponza.obj"; }

	const char* getDescription() const override;
	void drawOptionsUI() override;

	inline Image getRenderTarget() const override { return m_renderTarget; }
	inline glm::ivec2 getRenderTargetSize() const override { return glm::ivec2(m_width, m_height); }
};