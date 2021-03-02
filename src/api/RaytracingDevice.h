#pragma once

#include "RenderDevice.h"

#include <vector>
#include <memory>

struct RaytracingDeviceFeatures
{
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures = {};
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {};
	VkPhysicalDeviceBufferAddressFeaturesEXT bufferAddress = {};
	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexing = {};
	VkPhysicalDeviceHostQueryResetFeatures hostQueryReset = {};

	void* pNext = nullptr;
};

struct BLASGeometryInfo
{
	Buffer vertexBuffer;
	Buffer indexBuffer;

	std::vector<VkAccelerationStructureGeometryKHR> geometryArray;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> rangeInfoArray;
};

class RaytracingPipeline;

class BottomLevelAS;
class TopLevelAS;
class ShaderBindingTable;

class RaytracingDevice
{
private:
	VkPhysicalDeviceFeatures m_physicalDeviceFeatures = {};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR m_accelStructFeatures = {};
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_rtPipelineFeatures = {};
	VkPhysicalDeviceDescriptorIndexingFeatures m_descriptorIndexingFeatures = {};

	VkPhysicalDeviceProperties m_physicalDeviceProperties = {};
	VkPhysicalDeviceAccelerationStructurePropertiesKHR m_accelStructProperties = {};
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtPipelineProperties = {};

	RenderDevice* m_renderDevice = nullptr;
public:
	RaytracingDevice() {}
	~RaytracingDevice();

	RaytracingDeviceFeatures* init(RenderDevice* renderDevice);

	std::shared_ptr<const BLASGeometryInfo> compileGeometry(Buffer vertexBuffer, unsigned int vertexSize, unsigned int maxVertex, Buffer indexBuffer, unsigned int indexCount, VkDeviceOrHostAddressConstKHR transformData) const;
	VkAccelerationStructureInstanceKHR compileInstances(const BottomLevelAS& blas, glm::mat4 transform, uint32_t instanceCustomIndex, uint32_t mask, uint32_t instanceShaderBindingTableRecordOffset, VkGeometryInstanceFlagsKHR flags) const;

	void buildBLAS(std::vector<BottomLevelAS>& blasList) const;
	void buildTLAS(TopLevelAS& tlas, const std::vector<VkAccelerationStructureInstanceKHR>& instances, VkBuildAccelerationStructureFlagsKHR flags) const;

	std::vector<const char*> getRequiredExtensions() const;

	inline const RenderDevice* getRenderDevice() const { return m_renderDevice; }
	inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR getRTPipelineProperties() const { return m_rtPipelineProperties; }
};

class BottomLevelAS
{
private:
	VkAccelerationStructureKHR m_accelerationStructure;
	Buffer m_accelStorageBuffer;

	VkAccelerationStructureBuildGeometryInfoKHR m_buildInfo;
	VkAccelerationStructureBuildSizesInfoKHR m_sizeInfo;

	std::shared_ptr<const BLASGeometryInfo> m_geometryInfo = nullptr;

	const RaytracingDevice* m_device = nullptr;
public:
	void init(const RaytracingDevice* device, std::shared_ptr<const BLASGeometryInfo> geometryInfo, VkBuildAccelerationStructureFlagsKHR flags);
	void destroy();

	inline VkAccelerationStructureKHR get() const { return m_accelerationStructure; }

	inline const VkAccelerationStructureBuildSizesInfoKHR& getSizeInfo() const { return m_sizeInfo; }
	inline const VkAccelerationStructureBuildGeometryInfoKHR& getBuildInfo() const { return m_buildInfo; }
	inline std::shared_ptr<const BLASGeometryInfo> getGeometryInfo() const { return m_geometryInfo; }

	friend class RaytracingDevice;
};

class TopLevelAS
{
private:
	VkAccelerationStructureKHR m_accelerationStructure;
	Buffer m_accelStorageBuffer;

	const RaytracingDevice* m_device = nullptr;
public:
	void init(const RaytracingDevice* device);
	void destroy();

	inline VkAccelerationStructureKHR get() const { return m_accelerationStructure; }

	friend class RaytracingDevice;
};