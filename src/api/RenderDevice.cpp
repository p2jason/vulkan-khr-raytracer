#include "RenderDevice.h"

#include <iostream>
#include <algorithm>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	int minSeveritry = 1;

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
			std::cout << "Cannot find extension: " << extensions[i] << std::endl;
			return;
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
			std::cout << "Cannot find layer: " << extensions[i] << std::endl;
			return;
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
	}

	delete[] physicalDevices;

	if (bestDevice == VK_NULL_HANDLE)
	{
		std::cout << "Cannot find adequate GPU!" << std::endl;
		std::cin.get();
		std::exit(-1);
	}

	m_physicalDevice = bestDevice;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memProperties);
}

void RenderDevice::createLogicalDevice(std::vector<const char*> extensions, std::vector<const char*> validationLayers)
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
			std::cout << "Cannot find extension: " << extensions[i] << std::endl;
			return;
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
			std::cout << "Cannot find layer: " << extensions[i] << std::endl;
			return;
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
		std::cout << "Cannot find queue family that supports graphics operations!" << std::endl;
		std::cin.get();
		std::exit(-1);
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
	createInfo.queueCreateInfoCount = 1;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
	createInfo.ppEnabledLayerNames = validationLayers.data();
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.pEnabledFeatures = nullptr;

	VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

	volkLoadDevice(m_device);

	vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);
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

void RenderDevice::destroy()
{
	if (m_device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(m_device, nullptr);
	}

	if (m_surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	}

	if (m_instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(m_instance, nullptr);
	}
}