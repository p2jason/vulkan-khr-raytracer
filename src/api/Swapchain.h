#pragma once

#include <volk.h>
#include <vector>

struct SurfaceProfile
{
	VkSurfaceCapabilitiesKHR capabilties;

	std::vector<VkPresentModeKHR> presentModes;
	std::vector<VkSurfaceFormatKHR> surfaceFormats;

	bool extFullScreenSxclusive = false;
	bool amdDisplayNativeHdr = false;

	bool fullScreenExclusiveSupported = false;
	bool localDimmingSupported = false;
};

enum class FullScreenExclusiveMode
{
	DEFAULT = 0,
	DISALLOWED = 1,
	ALLOWED = 2,
	APPLICATION_CONTROLLED = 3
};

class Swapchain
{
private:
	VkDevice m_device = VK_NULL_HANDLE;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

	VkSurfaceFormatKHR m_format = { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	VkExtent2D m_extent = { 0, 0 };

	std::vector<VkImage> m_images;
	std::vector<VkImageView> m_imageViews;
public:
	Swapchain() {}

	uint32_t acquireNextImage(VkSemaphore semaphore) const;
	void present(VkQueue queue, uint32_t imageIndex, const std::vector<VkSemaphore>& waitSemaphores) const;

	void destroy();

	inline std::vector<VkImageView> getImageViews() const { return m_imageViews; }

	inline VkSurfaceFormatKHR getFormat() const { return m_format; }

	friend class SwapchainFactory;
};

class SwapchainFactory
{
private:
	struct
	{
		bool extSwapchainColorspace = false;
		bool khrGetSurfaceCapabilties2 = false;
	} m_instanceExtensions;

public:
	std::vector<const char*> determineInstanceExtensions();
	std::vector<const char*> determineDeviceExtensions(VkPhysicalDevice physicalDevice);

	SurfaceProfile createProfileForSurface(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& extensions, FullScreenExclusiveMode fullscreenMode, void* additionalData = nullptr) const;

	Swapchain createSwapchain(VkDevice device, VkSurfaceKHR surface, const SurfaceProfile& surfaceProfile, int width, int height) const;
};