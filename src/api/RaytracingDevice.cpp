#include "RaytracingDevice.h"

#include "api/RaytracingPipeline.h"

#include <glm/ext/matrix_transform.hpp>

#include <algorithm>
#include <iostream>

RaytracingDevice::~RaytracingDevice()
{

}

RaytracingDeviceFeatures* RaytracingDevice::init(RenderDevice* renderDevice)
{
	std::cout << "Initializing raytracing device" << std::endl;

	//Extract feature/property information
	m_physicalDeviceFeatures = {};

	m_descriptorIndexingFeatures = {};
	m_descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;

	m_accelStructFeatures = {};
	m_accelStructFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	m_accelStructFeatures.pNext = &m_descriptorIndexingFeatures;

	m_rtPipelineFeatures = {};
	m_rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	m_rtPipelineFeatures.pNext = &m_accelStructFeatures;

	m_physicalDeviceProperties = {};

	m_accelStructProperties = {};
	m_accelStructProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;

	m_rtPipelineProperties = {};
	m_rtPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	m_rtPipelineProperties.pNext = &m_accelStructProperties;

	renderDevice->getPhysicalDeviceFeatures(&m_physicalDeviceFeatures, &m_rtPipelineFeatures);
	renderDevice->getPhysicalDevicePropertes(&m_physicalDeviceProperties, &m_rtPipelineProperties);

	//Check features
	if (!m_accelStructFeatures.accelerationStructure || !m_accelStructFeatures.descriptorBindingAccelerationStructureUpdateAfterBind)
	{
		FATAL_ERROR("Required acceleration structure features not suported");
	}

	//Create feature struct
	RaytracingDeviceFeatures* features = new RaytracingDeviceFeatures();

	features->accelStructFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, nullptr, VK_TRUE, VK_FALSE, VK_FALSE, VK_FALSE, VK_TRUE };
	features->rtPipelineFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, &features->accelStructFeatures, VK_TRUE, VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE };
	features->bufferAddress = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_ADDRESS_FEATURES_EXT, &features->rtPipelineFeatures, VK_TRUE, VK_FALSE, VK_FALSE };

	features->descriptorIndexing = {};
	features->descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
	features->descriptorIndexing.pNext = &features->bufferAddress;
	features->descriptorIndexing.descriptorBindingVariableDescriptorCount = VK_TRUE;
	features->descriptorIndexing.runtimeDescriptorArray = VK_TRUE;
	features->descriptorIndexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	features->descriptorIndexing.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;

	features->hostQueryReset = {};
	features->hostQueryReset.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;
	features->hostQueryReset.pNext = &features->descriptorIndexing;
	features->hostQueryReset.hostQueryReset = VK_TRUE;

	features->scalarBlockLayout = {};
	features->scalarBlockLayout.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;
	features->scalarBlockLayout.pNext = &features->hostQueryReset;
	features->scalarBlockLayout.scalarBlockLayout = true;

	features->pNext = &features->scalarBlockLayout;

	m_renderDevice = renderDevice;

	return features;
}

std::shared_ptr<const BLASGeometryInfo> RaytracingDevice::compileGeometry(Buffer vertexBuffer, unsigned int vertexSize, unsigned int maxVertex, Buffer indexBuffer, unsigned int indexCount, VkDeviceOrHostAddressConstKHR transformData, VkGeometryFlagsKHR flags) const
{
	VkDeviceAddress vertexAddress = m_renderDevice->getBufferAddress(vertexBuffer.buffer);
	VkDeviceAddress indexAddress = m_renderDevice->getBufferAddress(indexBuffer.buffer);

	VkAccelerationStructureGeometryKHR geometry = {};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	geometry.geometry.triangles.vertexData.deviceAddress = vertexAddress;
	geometry.geometry.triangles.vertexStride = vertexSize;
	geometry.geometry.triangles.maxVertex = maxVertex;
	geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
	geometry.geometry.triangles.indexData.deviceAddress = indexAddress;
	geometry.geometry.triangles.transformData = transformData;
	geometry.flags = flags;

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	rangeInfo.firstVertex = 0;
	rangeInfo.primitiveCount = indexCount;
	rangeInfo.primitiveOffset = 0;
	rangeInfo.transformOffset = 0;

	std::shared_ptr<BLASGeometryInfo> geometryInfo = std::make_shared<BLASGeometryInfo>();

	geometryInfo->geometryArray.push_back(geometry);
	geometryInfo->rangeInfoArray.push_back(rangeInfo);

	return geometryInfo;
}

VkAccelerationStructureInstanceKHR RaytracingDevice::compileInstances(const BottomLevelAS& blas, glm::mat4 transform, uint32_t instanceCustomIndex, uint32_t mask, uint32_t sbtRecordOffset, VkGeometryInstanceFlagsKHR flags) const
{
	//Get acceleration structure address
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = blas.m_accelerationStructure;

	VkDeviceAddress accelAddress = vkGetAccelerationStructureDeviceAddressKHR(m_renderDevice->getDevice(), &addressInfo);

	VkAccelerationStructureInstanceKHR instance = {};
	instance.accelerationStructureReference = accelAddress;
	instance.instanceCustomIndex = instanceCustomIndex;
	instance.mask = mask;
	instance.instanceShaderBindingTableRecordOffset = sbtRecordOffset;
	instance.flags = flags;

	memcpy(instance.transform.matrix[0], &transform[0], 4 * sizeof(float));
	memcpy(instance.transform.matrix[1], &transform[1], 4 * sizeof(float));
	memcpy(instance.transform.matrix[2], &transform[2], 4 * sizeof(float));

	return instance;
}

void RaytracingDevice::buildBLAS(std::vector<BottomLevelAS>& blasList) const
{
	std::cout << "Building BLAS list (" << blasList.size() << "): ";
	std::cout << "Preparing... ";

	//Allocate scratch memory
	VkDeviceSize scratchSize = 0;

	for (size_t i = 0; i < blasList.size(); ++i)
	{
		scratchSize = std::max(blasList[i].getSizeInfo().buildScratchSize, scratchSize);
	}

	Buffer scratchMemory = m_renderDevice->createBuffer(scratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkDeviceAddress scratchAddress = m_renderDevice->getBufferAddress(scratchMemory.buffer);

	//Create query pool to store acceleration structure properties
	VkQueryPoolCreateInfo queryPoolCreateInfo = {};
	queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	queryPoolCreateInfo.queryCount = (uint32_t)blasList.size();
	queryPoolCreateInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;

	VkQueryPool queryPool;
	VK_CHECK(vkCreateQueryPool(m_renderDevice->getDevice(), &queryPoolCreateInfo, nullptr, &queryPool));

	vkResetQueryPool(m_renderDevice->getDevice(), queryPool, 0, (uint32_t)blasList.size());

	std::cout << "Building... ";

	//Create a single buffer for each BLAS to prevent the driver from getting stuck
	//if the workload is too load
	m_renderDevice->executeCommands((int)blasList.size(), [&](VkCommandBuffer* commandBuffers)
	{
		for (int i = 0; i < (int)blasList.size(); ++i)
		{
			VkAccelerationStructureBuildGeometryInfoKHR buildInfo = blasList[i].getBuildInfo();
			buildInfo.scratchData.deviceAddress = scratchAddress;

			std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> ppBuildRangeInfos(blasList[i].getGeometryInfo()->rangeInfoArray.size());
			std::transform(blasList[i].getGeometryInfo()->rangeInfoArray.cbegin(), blasList[i].getGeometryInfo()->rangeInfoArray.cend(), ppBuildRangeInfos.begin(), [](const VkAccelerationStructureBuildRangeInfoKHR& range) { return &range; });

			vkCmdBuildAccelerationStructuresKHR(commandBuffers[i], 1, &buildInfo, ppBuildRangeInfos.data());

			//Since all acceleration structures use the same scratch memory,
			//a barrier is need to prevent it from being used concurrently
			VkMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

			vkCmdPipelineBarrier(commandBuffers[i],
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				0, 1, &barrier, 0, nullptr, 0, nullptr);

			if ((buildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) == VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR)
			{
				VkAccelerationStructureKHR accelStruct = blasList[i].get();

				vkCmdWriteAccelerationStructuresPropertiesKHR(commandBuffers[i], 1, &accelStruct, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, i);
			}
		}
	});

	std::cout << "Compacting... ";

	std::vector<BottomLevelAS> oldBLASes = blasList;

	VkDeviceSize totalOriginalSize = 0;
	VkDeviceSize totalCompactSize = 0;

	m_renderDevice->executeCommands(1, [&](VkCommandBuffer* commandBuffer)
	{
		for (int i = 0; i < (int)blasList.size(); ++i)
		{
			VkAccelerationStructureBuildGeometryInfoKHR buildInfo = blasList[i].getBuildInfo();
			
			if ((buildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) != VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR)
			{
				continue;
			}

			VkDeviceSize compactSize = 0;
			VK_CHECK(vkGetQueryPoolResults(m_renderDevice->getDevice(), queryPool, i, 1, sizeof(VkDeviceSize), &compactSize, sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT));

			assert(compactSize != (VkDeviceSize)-1 && compactSize > 0);

			blasList[i].m_accelStorageBuffer = m_renderDevice->createBuffer(compactSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VkAccelerationStructureCreateInfoKHR createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
			createInfo.buffer = blasList[i].m_accelStorageBuffer.buffer;
			createInfo.offset = 0;
			createInfo.size = compactSize;
			createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

			VK_CHECK(vkCreateAccelerationStructureKHR(m_renderDevice->getDevice(), &createInfo, nullptr, &blasList[i].m_accelerationStructure));

			VkCopyAccelerationStructureInfoKHR copyInfo = {};
			copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
			copyInfo.src = oldBLASes[i].m_accelerationStructure;
			copyInfo.dst = blasList[i].m_accelerationStructure;
			copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;

			vkCmdCopyAccelerationStructureKHR(commandBuffer[0], &copyInfo);

			//Update structure size info
			totalOriginalSize += blasList[i].getSizeInfo().accelerationStructureSize;
			totalCompactSize += compactSize;
		}
	});

	std::cout << "Finished!" << std::endl;
	std::cout << "    -Total original size: " << (totalOriginalSize / 1024.0f) << "KB" << std::endl;
	std::cout << "    -Total compact size:  " << (totalCompactSize / 1024.0f) << "KB" << std::endl;
	std::cout << "    -Compaction ratio:    " << (float)((100.0 * totalCompactSize) / totalOriginalSize) << "%" << std::endl;;

	//Destroy resources
	for (int i = 0; i < (int)blasList.size(); ++i)
	{
		oldBLASes[i].destroy();
	}

	m_renderDevice->destroyBuffer(scratchMemory);

	vkDestroyQueryPool(m_renderDevice->getDevice(), queryPool, nullptr);
}

void RaytracingDevice::buildTLAS(TopLevelAS& tlas, const std::vector<VkAccelerationStructureInstanceKHR>& instances, VkBuildAccelerationStructureFlagsKHR flags) const
{
	std::cout << "Building TLAS: ";
	std::cout << "Preparing... ";

	VkDeviceSize instanceBufferSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

	//Allocate instance buffer
	Buffer instanceBuffer = m_renderDevice->createBuffer(instanceBufferSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VkDeviceAddress instanceAddress = m_renderDevice->getBufferAddress(instanceBuffer.buffer);

	//Copy instance content
	VkAccelerationStructureInstanceKHR* instanceMemory = nullptr;
	VK_CHECK(vkMapMemory(m_renderDevice->getDevice(), instanceBuffer.memory, 0, instanceBufferSize, 0, (void**)&instanceMemory));

	for (size_t i = 0; i < instances.size(); ++i)
	{
		memcpy(&instanceMemory[i], &instances[i], sizeof(VkAccelerationStructureInstanceKHR));
	}

	vkUnmapMemory(m_renderDevice->getDevice(), instanceBuffer.memory);

	std::cout << "Creating TLAS... ";

	//Create acceleration structre
	VkAccelerationStructureGeometryKHR topASGeometry = {};
	topASGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	topASGeometry.geometry.instances = {};
	topASGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	topASGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
	topASGeometry.geometry.instances.data.deviceAddress = instanceAddress;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.flags = flags;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &topASGeometry;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

	uint32_t count = (uint32_t)instances.size();

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	vkGetAccelerationStructureBuildSizesKHR(m_renderDevice->getDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &count, &sizeInfo);

	tlas.m_accelStorageBuffer = m_renderDevice->createBuffer(sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkAccelerationStructureCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.size = sizeInfo.accelerationStructureSize;
	createInfo.buffer = tlas.m_accelStorageBuffer.buffer;
	VK_CHECK(vkCreateAccelerationStructureKHR(m_renderDevice->getDevice(), &createInfo, nullptr, &tlas.m_accelerationStructure));

	//Create scratch memory
	Buffer scratchBuffer = m_renderDevice->createBuffer(sizeInfo.buildScratchSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkDeviceAddress scratchAddress = m_renderDevice->getBufferAddress(scratchBuffer.buffer);

	buildInfo.dstAccelerationStructure = tlas.m_accelerationStructure;
	buildInfo.scratchData.deviceAddress = scratchAddress;

	std::cout << "Building... ";

	//Build TLAS
	m_renderDevice->executeCommands(1, [&](VkCommandBuffer* commandBuffer)
	{
		VkMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

		vkCmdPipelineBarrier(commandBuffer[0],
							 VK_PIPELINE_STAGE_TRANSFER_BIT,
							 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
							 0, 1, &barrier, 0, nullptr, 0, nullptr);

		VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {};
		buildRangeInfo.primitiveCount = (uint32_t)instances.size();
		buildRangeInfo.primitiveOffset = 0;
		buildRangeInfo.firstVertex = 0;
		buildRangeInfo.transformOffset = 0;

		const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;

		vkCmdBuildAccelerationStructuresKHR(commandBuffer[0], 1, &buildInfo, &pBuildRangeInfo);
	});

	std::cout << "Finished!" << std::endl;
	std::cout << "    -Child instances: " << instances.size() << std::endl;

	m_renderDevice->destroyBuffer(instanceBuffer);
	m_renderDevice->destroyBuffer(scratchBuffer);
}

std::vector<const char*> RaytracingDevice::getRequiredExtensions() const
{
	return {
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_MAINTENANCE3_EXTENSION_NAME,
		VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
	};
}

void BottomLevelAS::init(const RaytracingDevice* device, std::shared_ptr<const BLASGeometryInfo> geometryInfo, VkBuildAccelerationStructureFlagsKHR flags)
{
	m_buildInfo = {};
	m_buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	m_buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	m_buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	m_buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	m_buildInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	m_buildInfo.geometryCount = (uint32_t)geometryInfo->geometryArray.size();
	m_buildInfo.pGeometries = geometryInfo->geometryArray.data();
	m_buildInfo.flags = flags;

	//Query acceleration structure size
	std::vector<uint32_t> maxPrimitiveCount(geometryInfo->rangeInfoArray.size());
	std::transform(geometryInfo->rangeInfoArray.cbegin(), geometryInfo->rangeInfoArray.cend(), maxPrimitiveCount.begin(), [](VkAccelerationStructureBuildRangeInfoKHR range) { return range.primitiveCount; });

	m_sizeInfo = {};
	m_sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	vkGetAccelerationStructureBuildSizesKHR(device->getRenderDevice()->getDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &m_buildInfo, maxPrimitiveCount.data(), &m_sizeInfo);

	//Allocate acceleration structure buffer
	m_accelStorageBuffer = device->getRenderDevice()->createBuffer(m_sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//Create acceleration structure
	VkAccelerationStructureCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.buffer = m_accelStorageBuffer.buffer;
	createInfo.offset = 0;
	createInfo.size = m_sizeInfo.accelerationStructureSize;
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

	VK_CHECK(vkCreateAccelerationStructureKHR(device->getRenderDevice()->getDevice(), &createInfo, nullptr, &m_accelerationStructure));

	m_buildInfo.dstAccelerationStructure = m_accelerationStructure;
	m_geometryInfo = geometryInfo;
	m_device = device;
}

void BottomLevelAS::destroy()
{
	if (m_accelerationStructure != VK_NULL_HANDLE)
	{
		vkDestroyAccelerationStructureKHR(m_device->getRenderDevice()->getDevice(), m_accelerationStructure, nullptr);
	}

	m_device->getRenderDevice()->destroyBuffer(m_accelStorageBuffer);
}

void TopLevelAS::init(const RaytracingDevice* device)
{
	m_device = device;
}

void TopLevelAS::destroy()
{
	if (m_accelerationStructure != VK_NULL_HANDLE)
	{
		vkDestroyAccelerationStructureKHR(m_device->getRenderDevice()->getDevice(), m_accelerationStructure, nullptr);
	}

	m_device->getRenderDevice()->destroyBuffer(m_accelStorageBuffer);
}