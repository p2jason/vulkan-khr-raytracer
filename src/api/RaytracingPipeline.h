#pragma once

#include "api/RaytracingDevice.h"

#include "scene/SceneLoader.h"
#include "camera/Camera.h"

#include <glm/gtc/quaternion.hpp>

#include <filesystem>
#include <string>
#include <memory>

struct HitGroupModules { VkShaderModule modules[3]; };

class RTPipelineInfo
{
public:
	std::vector<VkShaderModule> raygenModules;
	std::vector<VkShaderModule> missModules;
	std::vector<HitGroupModules> hitGroupModules;

	std::vector<VkDescriptorSetLayout> descSetLayouts;
	std::vector<VkPushConstantRange> pushConstants;

	int maxRecursionDepth = 1;

	std::unordered_map<std::string, std::filesystem::file_time_type> monitoredResources;

	bool failedToLoad = false;
private:
	void addResource(std::string resourcePath);
public:
	int addRaygenShaderFromPath(const RenderDevice* device, const char* raygenPath, std::vector<std::string> definitions = {});
	int addMissShaderFromPath(const RenderDevice* device, const char* missPath, std::vector<std::string> definitions = {});
	int addHitGroupFromPath(const RenderDevice* device, const char* closestHitPath, const char* anyHitPath = nullptr, const char* intersectionPath = nullptr,
														std::vector<std::string> closestHitDefs = {}, std::vector<std::string> anyHitDefs = {}, std::vector<std::string> intersectionDefs = {});
};

class RaytracingPipeline
{
protected:
	VkPipelineCache m_cache;
	std::shared_ptr<Scene> m_scene = nullptr;

	std::unordered_map<std::string, std::filesystem::file_time_type> m_monitoredResources;

	bool m_reloadPipeline = false;

	const RaytracingDevice* m_device = nullptr;
protected:
	void markReload() { m_reloadPipeline = true; }
public:
	virtual bool init(const RaytracingDevice* device, VkPipelineCache cache, std::shared_ptr<Scene> scene, std::shared_ptr<Camera> camera, std::shared_ptr<void> reloadOptions = nullptr) = 0;
	virtual void destroy() = 0;

	virtual void raytrace(VkCommandBuffer buffer) = 0;

	virtual void createRenderTarget(int width, int height) = 0;
	virtual void destroyRenderTarget() = 0;

	virtual Image getRenderTarget() const = 0;
	virtual glm::ivec2 getRenderTargetSize() const = 0;

	virtual std::string getDefaultScene() const = 0;
	virtual const char* getDescription() const { return "No description"; }
	virtual void drawOptionsUI() {}

	virtual void notifyCameraChange() {}

	virtual std::shared_ptr<void> getReloadOptions() const { return nullptr; }

	bool isOutOfDate() const;

	inline void notifyReloaded() { m_reloadPipeline = false; }
	inline bool shouldReload() const { return m_reloadPipeline; }
};

class NativeRaytracingPipeline : public RaytracingPipeline
{
private:
	uint32_t m_numRaygenShaders = 0;
	uint32_t m_numMissShaders = 0;
	uint32_t m_numHitGroups = 0;

	uint32_t m_sbtHandleSize = 0;
	uint32_t m_sbtHandleAlignedSize = 0;
	uint32_t m_sbtNumEntries = 0;
protected:
	VkPipeline m_pipeline = VK_NULL_HANDLE;
	VkPipelineLayout m_layout = VK_NULL_HANDLE;

	Buffer m_sbtBuffer;
protected:
	virtual bool create(const RaytracingDevice* device, RTPipelineInfo& pipelineInfo, std::shared_ptr<Camera> camera, std::shared_ptr<void> reloadOptions) = 0;
	virtual void clean(const RaytracingDevice* device) {}

	virtual void bind(VkCommandBuffer commandBuffer) {}
public:
	bool init(const RaytracingDevice* device, VkPipelineCache cache, std::shared_ptr<Scene> scene, std::shared_ptr<Camera> camera, std::shared_ptr<void> reloadOptions = nullptr) override;
	void destroy() override;

	void raytrace(VkCommandBuffer buffer) override;
};