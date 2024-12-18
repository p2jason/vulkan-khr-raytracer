#include "RenderDevice.h"

#include <iostream>
#include <algorithm>
#include <filesystem>
#include <sstream>

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <SPIRV/Logger.h>

#include "Common.h"

class BeginTerminateHook
{
public:
	BeginTerminateHook() { glslang::InitializeProcess(); }
	~BeginTerminateHook() { glslang::FinalizeProcess(); }
};

BeginTerminateHook s_beginTerminateHook;

const TBuiltInResource DefaultTBuiltInResource = {
	/* .MaxLights = */ 32,
	/* .MaxClipPlanes = */ 6,
	/* .MaxTextureUnits = */ 32,
	/* .MaxTextureCoords = */ 32,
	/* .MaxVertexAttribs = */ 64,
	/* .MaxVertexUniformComponents = */ 4096,
	/* .MaxVaryingFloats = */ 64,
	/* .MaxVertexTextureImageUnits = */ 32,
	/* .MaxCombinedTextureImageUnits = */ 80,
	/* .MaxTextureImageUnits = */ 32,
	/* .MaxFragmentUniformComponents = */ 4096,
	/* .MaxDrawBuffers = */ 32,
	/* .MaxVertexUniformVectors = */ 128,
	/* .MaxVaryingVectors = */ 8,
	/* .MaxFragmentUniformVectors = */ 16,
	/* .MaxVertexOutputVectors = */ 16,
	/* .MaxFragmentInputVectors = */ 15,
	/* .MinProgramTexelOffset = */ -8,
	/* .MaxProgramTexelOffset = */ 7,
	/* .MaxClipDistances = */ 8,
	/* .MaxComputeWorkGroupCountX = */ 65535,
	/* .MaxComputeWorkGroupCountY = */ 65535,
	/* .MaxComputeWorkGroupCountZ = */ 65535,
	/* .MaxComputeWorkGroupSizeX = */ 1024,
	/* .MaxComputeWorkGroupSizeY = */ 1024,
	/* .MaxComputeWorkGroupSizeZ = */ 64,
	/* .MaxComputeUniformComponents = */ 1024,
	/* .MaxComputeTextureImageUnits = */ 16,
	/* .MaxComputeImageUniforms = */ 8,
	/* .MaxComputeAtomicCounters = */ 8,
	/* .MaxComputeAtomicCounterBuffers = */ 1,
	/* .MaxVaryingComponents = */ 60,
	/* .MaxVertexOutputComponents = */ 64,
	/* .MaxGeometryInputComponents = */ 64,
	/* .MaxGeometryOutputComponents = */ 128,
	/* .MaxFragmentInputComponents = */ 128,
	/* .MaxImageUnits = */ 8,
	/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
	/* .MaxCombinedShaderOutputResources = */ 8,
	/* .MaxImageSamples = */ 0,
	/* .MaxVertexImageUniforms = */ 0,
	/* .MaxTessControlImageUniforms = */ 0,
	/* .MaxTessEvaluationImageUniforms = */ 0,
	/* .MaxGeometryImageUniforms = */ 0,
	/* .MaxFragmentImageUniforms = */ 8,
	/* .MaxCombinedImageUniforms = */ 8,
	/* .MaxGeometryTextureImageUnits = */ 16,
	/* .MaxGeometryOutputVertices = */ 256,
	/* .MaxGeometryTotalOutputComponents = */ 1024,
	/* .MaxGeometryUniformComponents = */ 1024,
	/* .MaxGeometryVaryingComponents = */ 64,
	/* .MaxTessControlInputComponents = */ 128,
	/* .MaxTessControlOutputComponents = */ 128,
	/* .MaxTessControlTextureImageUnits = */ 16,
	/* .MaxTessControlUniformComponents = */ 1024,
	/* .MaxTessControlTotalOutputComponents = */ 4096,
	/* .MaxTessEvaluationInputComponents = */ 128,
	/* .MaxTessEvaluationOutputComponents = */ 128,
	/* .MaxTessEvaluationTextureImageUnits = */ 16,
	/* .MaxTessEvaluationUniformComponents = */ 1024,
	/* .MaxTessPatchComponents = */ 120,
	/* .MaxPatchVertices = */ 32,
	/* .MaxTessGenLevel = */ 64,
	/* .MaxViewports = */ 16,
	/* .MaxVertexAtomicCounters = */ 0,
	/* .MaxTessControlAtomicCounters = */ 0,
	/* .MaxTessEvaluationAtomicCounters = */ 0,
	/* .MaxGeometryAtomicCounters = */ 0,
	/* .MaxFragmentAtomicCounters = */ 8,
	/* .MaxCombinedAtomicCounters = */ 8,
	/* .MaxAtomicCounterBindings = */ 1,
	/* .MaxVertexAtomicCounterBuffers = */ 0,
	/* .MaxTessControlAtomicCounterBuffers = */ 0,
	/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
	/* .MaxGeometryAtomicCounterBuffers = */ 0,
	/* .MaxFragmentAtomicCounterBuffers = */ 1,
	/* .MaxCombinedAtomicCounterBuffers = */ 1,
	/* .MaxAtomicCounterBufferSize = */ 16384,
	/* .MaxTransformFeedbackBuffers = */ 4,
	/* .MaxTransformFeedbackInterleavedComponents = */ 64,
	/* .MaxCullDistances = */ 8,
	/* .MaxCombinedClipAndCullDistances = */ 8,
	/* .MaxSamples = */ 4,
	/* .maxMeshOutputVerticesNV = */ 256,
	/* .maxMeshOutputPrimitivesNV = */ 512,
	/* .maxMeshWorkGroupSizeX_NV = */ 32,
	/* .maxMeshWorkGroupSizeY_NV = */ 1,
	/* .maxMeshWorkGroupSizeZ_NV = */ 1,
	/* .maxTaskWorkGroupSizeX_NV = */ 32,
	/* .maxTaskWorkGroupSizeY_NV = */ 1,
	/* .maxTaskWorkGroupSizeZ_NV = */ 1,
	/* .maxMeshViewCountNV = */ 4,
	/* .maxMeshOutputVerticesEXT = */ 256,
	/* .maxMeshOutputPrimitivesEXT = */ 256,
	/* .maxMeshWorkGroupSizeX_EXT = */ 128,
	/* .maxMeshWorkGroupSizeY_EXT = */ 128,
	/* .maxMeshWorkGroupSizeZ_EXT = */ 128,
	/* .maxTaskWorkGroupSizeX_EXT = */ 128,
	/* .maxTaskWorkGroupSizeY_EXT = */ 128,
	/* .maxTaskWorkGroupSizeZ_EXT = */ 128,
	/* .maxMeshViewCountEXT = */ 4,
	/* .maxDualSourceDrawBuffersEXT = */ 1,

	/* .limits = */ {
		/* .nonInductiveForLoops = */ 1,
		/* .whileLoops = */ 1,
		/* .doWhileLoops = */ 1,
		/* .generalUniformIndexing = */ 1,
		/* .generalAttributeMatrixVectorIndexing = */ 1,
		/* .generalVaryingIndexing = */ 1,
		/* .generalSamplerIndexing = */ 1,
		/* .generalVariableIndexing = */ 1,
		/* .generalConstantMatrixVectorIndexing = */ 1,
	} };

class ShaderIncluder : public glslang::TShader::Includer
{
public:
	IncludeResult* includeSystem(const char* headerName, const char* includerName, size_t inclusionDepth) override
	{
		return nullptr;
	}

	IncludeResult* includeLocal(const char* headerName, const char* includerName, size_t inclusionDepth) override
	{
		namespace fs = std::filesystem;

		fs::path fullPath = fs::path(Resources::shaderIncludeDir()) / headerName;
		std::string pathString = fullPath.generic_string();

		ShaderSource source = Resources::loadShader(pathString.c_str());

		if (!source.wasLoadedSuccessfully)
		{
			return nullptr;
		}

		std::string* content = new std::string(source.code);

		return new IncludeResult(pathString, content->c_str(), content->size(), (void*)content);
	}

	void releaseInclude(IncludeResult* result) override
	{
		if (!result)
		{
			return;
		}

		delete (std::string*)result->userData;
		delete result;
	}
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	int minSeveritry = 2;

	switch (messageSeverity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		if (minSeveritry <= 0)
		{
			std::cout << "Vulkan Info: " << pCallbackData->pMessage << std::endl;
		}
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		if (minSeveritry <= 1)
		{
			if (!!strcmp(pCallbackData->pMessage, "Added messenger"))
			{
				std::cout << "Vulkan Verbose: " << pCallbackData->pMessage << std::endl;
			}
		}
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		if (minSeveritry <= 2)
		{
			std::cout << "Vulkan Warning: " << pCallbackData->pMessage << std::endl;
		}
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		if (minSeveritry <= 3)
		{
			std::cout << "Vulkan Error: " << pCallbackData->pMessage << std::endl;
#ifndef NDEBUG
			__debugbreak();
#endif
		}
		break;
	}

	return VK_FALSE;
}

void RenderDevice::createInstance(std::vector<const char*> extensions, std::vector<const char*> validationLayers, bool enableDebugMessenger)
{
	if (enableDebugMessenger)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	//Check extenion support
	uint32_t extensionCount = 0;
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));

	VkExtensionProperties* instanceExtensions = new VkExtensionProperties[extensionCount];
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, instanceExtensions));

	for (size_t i = 0; i < extensions.size(); ++i)
	{
		bool found = false;

		for (uint32_t j = 0; j < extensionCount; ++j)
		{
			if (!strcmp(extensions[i], instanceExtensions[j].extensionName))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			FATAL_ERROR("Cannot find extension: %s", extensions[i]);
		}
	}

	//Check layer support
	uint32_t layerCount = 0;
	VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

	VkLayerProperties* instanceLayers = new VkLayerProperties[layerCount];
	VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, instanceLayers));

	for (size_t i = 0; i < validationLayers.size(); ++i)
	{
		bool found = false;

		for (uint32_t j = 0; j < layerCount; ++j)
		{
			if (!strcmp(validationLayers[i], instanceLayers[j].layerName))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			FATAL_ERROR("Cannot find layer: %s", validationLayers[i]);
		}
	}

	//Create instance
	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pApplicationName = "Vulkan KHR Raytracer";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.pEngineName = "Vulkan KHR Raytracer";
	applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.apiVersion = VK_API_VERSION_1_2;

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugCreateInfo.pNext = nullptr;
	debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = debugCallback;
	debugCreateInfo.pUserData = nullptr;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = enableDebugMessenger ? &debugCreateInfo : nullptr;
	createInfo.pApplicationInfo = &applicationInfo;
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledLayerNames = validationLayers.data();
	createInfo.enabledLayerCount = (uint32_t)validationLayers.size();

	VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));

	volkLoadInstance(m_instance);

	if (enableDebugMessenger)
	{
		debugCreateInfo = {};
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.pNext = nullptr;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = debugCallback;
		debugCreateInfo.pUserData = nullptr;

		VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_instance, &debugCreateInfo, nullptr, &m_debugMessenger));
	}
}

void RenderDevice::createSurface(const Window& window)
{
	m_surface = window.createSurface(m_instance);
}

void RenderDevice::choosePhysicalDevice()
{
	uint32_t count = 0;
	VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &count, nullptr));

	VkPhysicalDevice* physicalDevices = new VkPhysicalDevice[count];
	VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &count, physicalDevices));

	VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
	for (uint32_t i = 0; i < count; ++i)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);

		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(physicalDevices[i], &features);

		bestDevice = physicalDevices[i];

		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			break;
	}

	delete[] physicalDevices;

	if (bestDevice == VK_NULL_HANDLE)
	{
		FATAL_ERROR("Cannot find adequate GPU!");
	}

	m_physicalDevice = bestDevice;
	
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memProperties);

	printf("Chosen physical device: %s\n", properties.deviceName);
}

void RenderDevice::getPhysicalDeviceFeatures(VkPhysicalDeviceFeatures* features, void* pNextChain) const
{
	VkPhysicalDeviceFeatures2 features2 = {};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = pNextChain;

	vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features2);

	if (features)
	{
		*features = features2.features;
	}
}

void RenderDevice::getPhysicalDevicePropertes(VkPhysicalDeviceProperties* properties, void* pNextChain) const
{
	VkPhysicalDeviceProperties2	properties2 = {};
	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = pNextChain;

	vkGetPhysicalDeviceProperties2(m_physicalDevice, &properties2);

	if (properties)
	{
		*properties = properties2.properties;
	}
}

void RenderDevice::createLogicalDevice(std::vector<const char*> extensions, std::vector<const char*> validationLayers, void* pNextChain, VkPhysicalDeviceFeatures* pFeatures)
{
	//Check extension support
	uint32_t extensionCount = 0;
	VK_CHECK(vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, nullptr));

	VkExtensionProperties* instanceExtensions = new VkExtensionProperties[extensionCount];
	VK_CHECK(vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, instanceExtensions));

	for (size_t i = 0; i < extensions.size(); ++i)
	{
		bool found = false;

		for (uint32_t j = 0; j < extensionCount; ++j)
		{
			if (!strcmp(extensions[i], instanceExtensions[j].extensionName))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			FATAL_ERROR("Cannot find extension: %s", extensions[i]);
		}
	}

	//Check layer support
	uint32_t layerCount = 0;
	VK_CHECK(vkEnumerateDeviceLayerProperties(m_physicalDevice, &layerCount, nullptr));

	VkLayerProperties* deviceLayers = new VkLayerProperties[layerCount];
	VK_CHECK(vkEnumerateDeviceLayerProperties(m_physicalDevice, &layerCount, deviceLayers));

	for (size_t i = 0; i < validationLayers.size(); ++i)
	{
		bool found = false;

		for (uint32_t j = 0; j < layerCount; ++j)
		{
			if (!strcmp(validationLayers[i], deviceLayers[j].layerName))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			FATAL_ERROR("Cannot find layer: %s", validationLayers[i]);
		}
	}

	//Choose queue family
	uint32_t queueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, nullptr);

	VkQueueFamilyProperties* queueFamilies = new VkQueueFamilyProperties[queueCount];
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, queueFamilies);

	for (uint32_t i = 0; i < queueCount; ++i)
	{
		VkBool32 presentSupported = VK_FALSE;
		VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupported));
																			    
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupported == VK_TRUE)
		{
			m_queueFamilyIndex = i;
		}
	}

	delete[] queueFamilies;

	if (m_queueFamilyIndex == (uint32_t)-1)
	{
		FATAL_ERROR("Cannot find queue family that supports graphics operations!");
	}

	float priority = 1.0f;

	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &priority;

	//Create logical device
	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pNext = pNextChain;
	createInfo.queueCreateInfoCount = 1;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
	createInfo.ppEnabledLayerNames = validationLayers.data();
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.pEnabledFeatures = pFeatures;

	VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

	volkLoadDevice(m_device);

	vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);

	//Create transient command pool
	m_transientPool = createCommandPool(VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
}

VkCommandPool RenderDevice::createCommandPool(VkCommandPoolCreateFlags flags) const
{
	VkCommandPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = m_queueFamilyIndex;
	createInfo.flags = flags;

	VkCommandPool commandPool;
	VK_CHECK(vkCreateCommandPool(m_device, &createInfo, nullptr, &commandPool));

	return commandPool;
}

void RenderDevice::submit(const std::vector<VkCommandBuffer>& commandBuffers, const std::vector<std::pair<VkSemaphore, VkPipelineStageFlags>>& waitSemaphores, const std::vector<VkSemaphore> signalSemaphores, VkFence signalFence) const
{
	std::vector<VkSemaphore> wSemaphores(waitSemaphores.size());
	std::vector<VkPipelineStageFlags> wStages(waitSemaphores.size());

	for (size_t i = 0; i < waitSemaphores.size(); ++i)
	{
		wSemaphores[i] = waitSemaphores[i].first;
		wStages[i] = waitSemaphores[i].second;
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
	submitInfo.pWaitSemaphores = wSemaphores.data();
	submitInfo.pWaitDstStageMask = wStages.data();
	submitInfo.commandBufferCount = (uint32_t)commandBuffers.size();
	submitInfo.pCommandBuffers = commandBuffers.data();
	submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	VK_CHECK(vkQueueSubmit(m_queue, 1, &submitInfo, signalFence));
}

uint32_t RenderDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
	for (uint32_t i = 0; i < m_memProperties.memoryTypeCount; ++i)
	{
		if (typeFilter & (1 << i) && (m_memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	return (uint32_t)-1;
}

Buffer RenderDevice::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) const
{
	VkBufferCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.size = size;
	createInfo.usage = usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;
	
	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(m_device, &createInfo, nullptr, &buffer));

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(m_device, buffer, &requirements);

	VkMemoryAllocateFlagsInfo allocFlags = {};
	allocFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &allocFlags : nullptr;
	allocInfo.allocationSize = requirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, properties);

	if (allocInfo.memoryTypeIndex == (uint32_t)-1)
	{
		FATAL_ERROR("Could not find appropriate memory type for buffer");
	}

	VkDeviceMemory memory;
	VK_CHECK(vkAllocateMemory(m_device, &allocInfo, nullptr, &memory));
	VK_CHECK(vkBindBufferMemory(m_device, buffer, memory, 0));

	return { buffer, memory };
}

VkDeviceAddress RenderDevice::getBufferAddress(VkBuffer buffer) const
{
	VkBufferDeviceAddressInfo addressInfo = {};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = buffer;

	return vkGetBufferDeviceAddress(m_device, &addressInfo);;
}

void RenderDevice::destroyBuffer(const Buffer& buffer) const
{
	if (buffer.buffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(m_device, buffer.buffer, nullptr);
	}

	if (buffer.memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(m_device, buffer.memory, nullptr);
	}
}

Image RenderDevice::createImage2D(int width, int height, VkFormat format, int mipLevels, VkImageUsageFlags usage, VkMemoryPropertyFlags properties) const
{
	VkImageCreateInfo imageCI = {};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = format;
	imageCI.extent = { (uint32_t)width, (uint32_t)height, 1 };
	imageCI.mipLevels = mipLevels;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = usage;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImage image;
	VK_CHECK(vkCreateImage(m_device, &imageCI, nullptr, &image));

	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(m_device, image, &requirements);
	
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = requirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, properties);

	if (allocInfo.memoryTypeIndex == (uint32_t)-1)
	{
		FATAL_ERROR("Could not find appropriate memory type for image");
	}

	VkDeviceMemory memory;
	VK_CHECK(vkAllocateMemory(m_device, &allocInfo, nullptr, &memory));
	VK_CHECK(vkBindImageMemory(m_device, image, memory, 0));

	VkImageViewCreateInfo imageViewCI = {};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.image = image;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.format = format;
	imageViewCI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCI.subresourceRange.baseMipLevel = 0;
	imageViewCI.subresourceRange.levelCount = 1;
	imageViewCI.subresourceRange.baseArrayLayer = 0;
	imageViewCI.subresourceRange.layerCount = 1;
	
	VkImageView imageView;
	VK_CHECK(vkCreateImageView(m_device, &imageViewCI, nullptr, &imageView));

	return { image, memory, imageView };
}

void RenderDevice::destroyImage(const Image& image) const
{
	if (image.imageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(m_device, image.imageView, nullptr);
	}

	if (image.image != VK_NULL_HANDLE)
	{
		vkDestroyImage(m_device, image.image, nullptr);
	}

	if (image.memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(m_device, image.memory, nullptr);
	}
}

void RenderDevice::executeCommands(int bufferCount, const std::function<void(VkCommandBuffer*)>& func) const
{
	//Create fence
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	VkFence buildCompleteFence;
	VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &buildCompleteFence));

	//Allocate command buffers
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_transientPool;
	allocInfo.commandBufferCount = bufferCount;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	std::vector<VkCommandBuffer> commandBuffers(bufferCount);
	VK_CHECK(vkAllocateCommandBuffers(m_device, &allocInfo, commandBuffers.data()));

	//Record command buffers
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	for (int i = 0; i < bufferCount; ++i)
	{
		VK_CHECK(vkBeginCommandBuffer(commandBuffers[i], &beginInfo));
	}

	func(commandBuffers.data());

	for (int i = 0; i < bufferCount; ++i)
	{
		VK_CHECK(vkEndCommandBuffer(commandBuffers[i]));
	}

	std::lock_guard<std::mutex> guard(*m_queueSubmitMutex);

	//Submit comamnd buffers
	submit(commandBuffers, {}, {}, buildCompleteFence);

	VK_CHECK(vkWaitForFences(m_device, 1, &buildCompleteFence, VK_TRUE, UINT64_MAX));

	//Free resources
	vkFreeCommandBuffers(m_device, m_transientPool, (uint32_t)commandBuffers.size(), commandBuffers.data());

	vkDestroyFence(m_device, buildCompleteFence, nullptr);
}

VkShaderModule RenderDevice::compileShader(VkShaderStageFlagBits shaderType, const std::string& source, const std::vector<std::string>& definitions) const
{
	using namespace glslang;

	//Select shader language
	EShLanguage language;
	const char* shaderName = "";

	const char* shaderSource = source.c_str();
	int shaderLength = (int)source.size();

	switch (shaderType)
	{
	case VK_SHADER_STAGE_VERTEX_BIT:
		language = EShLangVertex; shaderName = "Vertex";
		break;
	case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		language = EShLangTessControl; shaderName = "TessControl";
		break;
	case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		language = EShLangTessEvaluation; shaderName = "TessEvaluation";
		break;
	case VK_SHADER_STAGE_GEOMETRY_BIT:
		language = EShLangGeometry; shaderName = "Geometry";
		break;
	case VK_SHADER_STAGE_FRAGMENT_BIT:
		language = EShLangFragment; shaderName = "Fragment";
		break;
	case VK_SHADER_STAGE_COMPUTE_BIT:
		language = EShLangCompute; shaderName = "Compute";
		break;
	case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
		language = EShLangRayGen; shaderName = "RayGen";
		break;
	case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
		language = EShLangAnyHit; shaderName = "AnyHit";
		break;
	case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		language = EShLangClosestHit; shaderName = "ClosestHit";
		break;
	case VK_SHADER_STAGE_MISS_BIT_KHR:
		language = EShLangMiss; shaderName = "Miss";
		break;
	case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		language = EShLangIntersect; shaderName = "Intersect";
		break;
	case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
		language = EShLangCallable; shaderName = "Callable";
		break;
	case VK_SHADER_STAGE_TASK_BIT_NV:
		language = EShLangTaskNV; shaderName = "TaskNV";
		break;
	case VK_SHADER_STAGE_MESH_BIT_NV:
		language = EShLangMeshNV; shaderName = "MeshNV";
		break;
	default:
		std::cerr << "Unrecognized shader type: " << shaderType << std::endl;
		return VK_NULL_HANDLE;
	}
	
	std::stringstream ss;

	ss << "#extension GL_GOOGLE_include_directive : require\n";

	for (std::string define : definitions)
	{
		ss << define << "\n";
	}

	std::string preamble = ss.str();

	//Compile shader
	EShMessages messages = (EShMessages)(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);

	const int defaultVersion = 460;

	std::unique_ptr<TShader> shader = std::make_unique<TShader>(language);
	shader->setStringsWithLengths(&shaderSource, &shaderLength, 1);
	shader->setEnvInput(EShSourceGlsl, language, EShClientVulkan, defaultVersion);
	shader->setEnvClient(EShClientVulkan, EShTargetVulkan_1_2);
	shader->setEnvTarget(EshTargetSpv, EShTargetSpv_1_5);
	shader->setPreamble(preamble.c_str());

	ShaderIncluder includer;
	if (!shader->parse(&DefaultTBuiltInResource, defaultVersion, false, messages, includer))
	{
		std::cerr << "Error generated when compiling " << shaderName << " shader:" << std::endl;
		std::cerr << shader->getInfoLog() << shader->getInfoDebugLog();

		return VK_NULL_HANDLE;
	}

	std::unique_ptr<TProgram> program = std::make_unique<TProgram>();
	program->addShader(shader.get());

	if (!program->link(messages) || !program->mapIO())
	{
		std::cerr << "Error generated when compiling " << shaderName << " shader:" << std::endl;
		std::cerr << shader->getInfoLog() << shader->getInfoDebugLog();

		return VK_NULL_HANDLE;
	}

	std::vector<uint32_t> spirv;
	spv::SpvBuildLogger logger;

	SpvOptions spvOptions;
	spvOptions.generateDebugInfo = false;
	spvOptions.disableOptimizer = false;
	spvOptions.optimizeSize = false;
	spvOptions.disassemble = false;
	spvOptions.validate = false;
	GlslangToSpv(*program->getIntermediate(language), spirv, &logger, &spvOptions);

	if (logger.getAllMessages().size() > 0)
	{
		std::cout << "Message generated when compiling " << shaderName << " shader:" << std::endl;
		std::cout << logger.getAllMessages();
	}

	//Create shader module
	VkShaderModuleCreateInfo moduleCI = {};
	moduleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCI.pCode = spirv.data();
	moduleCI.codeSize = spirv.size() * sizeof(uint32_t);

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	VK_CHECK(vkCreateShaderModule(m_device, &moduleCI, nullptr, &shaderModule));

	return shaderModule;
}

void RenderDevice::destroy()
{
	if (m_transientPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(m_device, m_transientPool, nullptr);
	}

	if (m_device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(m_device, nullptr);
	}

	if (m_surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	}

	if (m_debugMessenger != VK_NULL_HANDLE)
	{
		vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
	}

	if (m_instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(m_instance, nullptr);
	}
}