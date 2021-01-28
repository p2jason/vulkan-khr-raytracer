#include "Swapchain.h"

#include "RenderDevice.h"

#include <algorithm>

std::vector<const char*> SwapchainFactory::determineInstanceExtensions()
{
	uint32_t instanceExtensions;
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensions, nullptr));

	std::vector<VkExtensionProperties> supportedExtensions(instanceExtensions);
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensions, supportedExtensions.data()));

	std::vector<const char*> requiredExtensions;

	for (uint32_t i = 0; i < instanceExtensions; ++i)
	{
		const char* name = supportedExtensions[i].extensionName;

		if (!strcmp(name, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME))
		{
			requiredExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);

			m_instanceExtensions.extSwapchainColorspace = true;
		}
		else if(!strcmp(name, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME))
		{
			requiredExtensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);

			m_instanceExtensions.khrGetSurfaceCapabilties2 = true;
		}
	}

	return requiredExtensions;
}

std::vector<const char*> SwapchainFactory::determineDeviceExtensions(VkPhysicalDevice physicalDevice)
{
	uint32_t deviceExtensions;
	VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensions, nullptr));

	std::vector<VkExtensionProperties> supportedExtensions(deviceExtensions);
	VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensions, supportedExtensions.data()));

	std::vector<const char*> requiredExtensions;

	for (uint32_t i = 0; i < deviceExtensions; ++i)
	{
		const char* name = supportedExtensions[i].extensionName;

		if (!strcmp(name, VK_AMD_DISPLAY_NATIVE_HDR_EXTENSION_NAME))
		{
			requiredExtensions.push_back(VK_AMD_DISPLAY_NATIVE_HDR_EXTENSION_NAME);
		}
		else if (!strcmp(name, VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME))
		{
			requiredExtensions.push_back(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME);
		}
	}

	return requiredExtensions;
}

SurfaceProfile SwapchainFactory::createProfileForSurface(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& extensions, FullScreenExclusiveMode fullscreenMode, void* additionalData) const
{
	SurfaceProfile profile = {};

	for (const char* ext : extensions)
	{
		if (!strcmp(ext, VK_AMD_DISPLAY_NATIVE_HDR_EXTENSION_NAME))
		{
			profile.amdDisplayNativeHdr = true;
		}
#if defined(VK_EXT_full_screen_exclusive)
		else if (!strcmp(ext, VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME))
		{
			profile.extFullScreenSxclusive = true;
		}
#endif
	}

	if (m_instanceExtensions.khrGetSurfaceCapabilties2)
	{
		assert(fullscreenMode != FullScreenExclusiveMode::APPLICATION_CONTROLLED || additionalData);

		//Input data structures
		void* pChainHead = nullptr;

#if defined(VK_EXT_full_screen_exclusive)
#if defined(VK_KHR_win32_surface)
		VkSurfaceFullScreenExclusiveWin32InfoEXT surfaceExclusiveWin32Info = {};
		surfaceExclusiveWin32Info.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT;
		surfaceExclusiveWin32Info.pNext = pChainHead;
		surfaceExclusiveWin32Info.hmonitor = additionalData ? (*(HMONITOR*)additionalData) : 0;

		pChainHead = additionalData ? &surfaceExclusiveWin32Info : pChainHead;
#endif

		VkSurfaceFullScreenExclusiveInfoEXT surfaceExclusiveInfo = {};
		surfaceExclusiveInfo.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;
		surfaceExclusiveInfo.pNext = pChainHead;
		surfaceExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_DEFAULT_EXT;

		pChainHead = profile.extFullScreenSxclusive ? &surfaceExclusiveInfo : nullptr;

		switch (fullscreenMode)
		{
		default:case FullScreenExclusiveMode::DEFAULT: 
			surfaceExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_DEFAULT_EXT;
			break;
		case FullScreenExclusiveMode::DISALLOWED:
			surfaceExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT;
			break;
		case FullScreenExclusiveMode::ALLOWED:
			surfaceExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT;
			break;
		case FullScreenExclusiveMode::APPLICATION_CONTROLLED:
			surfaceExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;
			break;
		}
#endif

		VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {};
		surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
		surfaceInfo.pNext = pChainHead;
		surfaceInfo.surface = surface;

		//Output data structures
		pChainHead = nullptr;

		VkDisplayNativeHdrSurfaceCapabilitiesAMD displayNativeHdr = {};
		displayNativeHdr.sType = VK_STRUCTURE_TYPE_DISPLAY_NATIVE_HDR_SURFACE_CAPABILITIES_AMD;
		displayNativeHdr.pNext = pChainHead;

		pChainHead = profile.amdDisplayNativeHdr ? &displayNativeHdr : nullptr;

#if defined(VK_EXT_full_screen_exclusive)
		VkSurfaceCapabilitiesFullScreenExclusiveEXT surfaceExclusiveCapabilities = {};
		surfaceExclusiveCapabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT;
		surfaceExclusiveCapabilities.pNext = pChainHead;

		pChainHead = profile.extFullScreenSxclusive ? &surfaceExclusiveCapabilities : nullptr;
#endif

		VkSurfaceCapabilities2KHR surfaceCapabilties = {};
		surfaceCapabilties.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
		surfaceCapabilties.pNext = pChainHead;

		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilities2KHR(device, &surfaceInfo, &surfaceCapabilties));

		//Pull surface details
		profile.capabilties = surfaceCapabilties.surfaceCapabilities;
		profile.localDimmingSupported = displayNativeHdr.localDimmingSupport;
#if defined(VK_EXT_full_screen_exclusive)
		profile.fullScreenExclusiveSupported = surfaceExclusiveCapabilities.fullScreenExclusiveSupported;

		//Get present modes
		uint32_t presentModeCount = 0;
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModes2EXT(device, &surfaceInfo, &presentModeCount, nullptr));

		profile.presentModes.resize(presentModeCount);
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModes2EXT(device, &surfaceInfo, &presentModeCount, profile.presentModes.data()));
#else
		profile.fullScreenExclusiveSupported = false;

		//Get present modes
		uint32_t presentModeCount = 0;
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr));

		profile.presentModes.resize(presentModeCount);
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, profile.presentModes.data()));
#endif
	}
	else
	{
		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &profile.capabilties));

		//Get present modes
		uint32_t presentModeCount = 0;
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr));

		profile.presentModes.resize(presentModeCount);
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, profile.presentModes.data()));
	}

	//Get surface formats
	uint32_t formatCount = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr));

	profile.surfaceFormats.resize(formatCount);
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, profile.surfaceFormats.data()));
	
	return profile;
}

Swapchain SwapchainFactory::createSwapchain(VkDevice device, VkSurfaceKHR surface, const SurfaceProfile& surfaceProfile, int width, int height) const
{
	//Choose surface format
	VkSurfaceFormatKHR scFormat = surfaceProfile.surfaceFormats[0];
	for (VkSurfaceFormatKHR format : surfaceProfile.surfaceFormats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			scFormat = format;
			break;
		}
	}

	//Choose present mode
	VkPresentModeKHR scPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (VkPresentModeKHR mode : surfaceProfile.presentModes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			scPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
	}

	VkExtent2D scExtent = surfaceProfile.capabilties.currentExtent;
	if (surfaceProfile.capabilties.currentExtent.width == UINT32_MAX || surfaceProfile.capabilties.currentExtent.height == UINT32_MAX)
	{
		scExtent.width = std::max(surfaceProfile.capabilties.minImageExtent.width, std::min((uint32_t)width, surfaceProfile.capabilties.maxImageExtent.width));
		scExtent.height = std::max(surfaceProfile.capabilties.minImageExtent.height, std::min((uint32_t)height, surfaceProfile.capabilties.maxImageExtent.height));
	}

	//Choose image count
	uint32_t imageCount = surfaceProfile.capabilties.minImageCount + 1;
	if (surfaceProfile.capabilties.maxImageCount > 0 && imageCount > surfaceProfile.capabilties.maxImageCount)
	{
		imageCount = surfaceProfile.capabilties.maxImageCount;
	}

	//Create swapchain
	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = scFormat.format;
	createInfo.imageColorSpace = scFormat.colorSpace;
	createInfo.imageExtent = scExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;
	createInfo.preTransform = surfaceProfile.capabilties.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = scPresentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	Swapchain swapchain;
	VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain.m_swapchain));

	swapchain.m_device = device;
	swapchain.m_format = scFormat;
	swapchain.m_extent = scExtent;

	//Pull swapchain images
	uint32_t swapchainImageCount = 0;
	VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain.m_swapchain, &swapchainImageCount, nullptr));

	swapchain.m_images.resize(swapchainImageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain.m_swapchain, &swapchainImageCount, swapchain.m_images.data()));

	//Create swapchain image views
	swapchain.m_imageViews.resize(swapchainImageCount);

	for (size_t i = 0; i < swapchainImageCount; ++i)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapchain.m_images[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapchain.m_format.format;
		createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;

		VkImageView imageView;
		VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageView));

		swapchain.m_imageViews[i] = imageView;
	}

	return swapchain;
}

uint32_t Swapchain::acquireNextImage(VkSemaphore semaphore) const
{
	uint32_t imageIndex = 0;
	VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &imageIndex));

	return imageIndex;
}

void Swapchain::present(VkQueue queue, uint32_t imageIndex, const std::vector<VkSemaphore>& waitSemaphores) const
{
	VkResult result = VK_SUCCESS;

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
	presentInfo.pWaitSemaphores = waitSemaphores.data();
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = &result;

	VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));
}

void Swapchain::destroy()
{
	for (size_t i = 0; i < m_imageViews.size(); ++i)
	{
		vkDestroyImageView(m_device, m_imageViews[i], nullptr);
	}

	m_imageViews.clear();
	m_images.clear();

	if (m_swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	}
}