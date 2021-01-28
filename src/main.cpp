#include <iostream>
#include <thread>
#include <chrono>

#include <volk.h>

#include "api/Window.h"
#include "api/RenderDevice.h"

#include "api/Swapchain.h"

#include <glm/glm.hpp>

std::vector<VkFramebuffer> createFramebuffers(VkDevice device, int width, int height, const std::vector<VkImageView>& imageViews, VkRenderPass renderPass)
{
	std::vector<VkFramebuffer> framebuffers(imageViews.size());

	for (size_t i = 0; i < imageViews.size(); ++i)
	{
		VkFramebufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = renderPass;
		createInfo.attachmentCount = 1;
		createInfo.pAttachments = &imageViews[i];
		createInfo.width = width;
		createInfo.height = height;
		createInfo.layers = 1;
		
		VK_CHECK(vkCreateFramebuffer(device, &createInfo, nullptr, &framebuffers[i]));
	}

	return framebuffers;
}

void destroyFramebuffers(VkDevice device, std::vector<VkFramebuffer>& framebuffers)
{
	for (size_t i = 0; i < framebuffers.size(); ++i)
	{
		vkDestroyFramebuffer(device, framebuffers[i], nullptr);
	}
}

int main()
{
	Window window;
	if (!window.init("Vulkan KHR Raytracer", 1280, 720))
	{
		std::cin.get();
		return 0;
	}

	//Setup render device
	SwapchainFactory swapchainFactory;
	std::vector<const char*> extensions = swapchainFactory.determineInstanceExtensions();

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = window.getRequiredExtensions(glfwExtensionCount);
	extensions.insert(extensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);

	std::vector<const char*> validationLayers({ "VK_LAYER_LUNARG_standard_validation", "VK_LAYER_KHRONOS_validation" });

	glm::ivec2 viewportSize = window.getViewportSize();

	RenderDevice renderDevice;
	renderDevice.createInstance(extensions, validationLayers, true);
	renderDevice.createSurface(window);
	renderDevice.choosePhysicalDevice();

	std::vector<const char*> deviceExtensions = swapchainFactory.determineDeviceExtensions(renderDevice.getPhysicalDevice());
	deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	renderDevice.createLogicalDevice(deviceExtensions, validationLayers);
	
	SurfaceProfile profile = swapchainFactory.createProfileForSurface(renderDevice.getPhysicalDevice(), renderDevice.getSurface(), {}, FullScreenExclusiveMode::DEFAULT, nullptr);
	Swapchain swapchain = swapchainFactory.createSwapchain(renderDevice.getDevice(), renderDevice.getSurface(), profile, viewportSize.x, viewportSize.y);

	//Create command pool & buffer
	VkDevice device = renderDevice.getDevice();
	VkCommandPool commandPool = renderDevice.createCommandPool();

	VkCommandBuffer commandBuffer;
	{
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		
		VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));
	}

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderCompleteSemaphore;
	VkFence renderFinishedFence;

	{
		VkSemaphoreCreateInfo semaphoreCI = {};
		semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		
		VK_CHECK(vkCreateSemaphore(device, &semaphoreCI, nullptr, &imageAvailableSemaphore));
		VK_CHECK(vkCreateSemaphore(device, &semaphoreCI, nullptr, &renderCompleteSemaphore));

		VkFenceCreateInfo fenceCI = {};
		fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		
		VK_CHECK(vkCreateFence(device, &fenceCI, nullptr, &renderFinishedFence));
	}

	VkRenderPass renderPass;
	{
		VkAttachmentDescription attachments[1] = {};
		attachments[0].format = swapchain.getFormat().format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		VkAttachmentReference references[1] = {};
		references[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpasses[1] = {};
		subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpasses[0].inputAttachmentCount = 0;
		subpasses[0].pInputAttachments = nullptr;
		subpasses[0].colorAttachmentCount = 1;
		subpasses[0].pColorAttachments = &references[0];
		subpasses[0].pResolveAttachments = nullptr;
		subpasses[0].pDepthStencilAttachment = nullptr;
		subpasses[0].preserveAttachmentCount = 0;
		subpasses[0].pPreserveAttachments = nullptr;
		
		VkSubpassDependency dependencies[1] = {};
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = 0;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		
		VkRenderPassCreateInfo renderPassCI = {};
		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCI.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
		renderPassCI.pAttachments = attachments;
		renderPassCI.subpassCount = sizeof(subpasses) / sizeof(subpasses[0]);
		renderPassCI.pSubpasses = subpasses;
		renderPassCI.dependencyCount = sizeof(dependencies) / sizeof(dependencies[0]);
		renderPassCI.pDependencies = dependencies;
		
		VK_CHECK(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderPass));
	}

	std::vector<VkFramebuffer> framebuffers = createFramebuffers(device, viewportSize.x, viewportSize.y, swapchain.getImageViews(), renderPass);

	while (!window.isCloseRequested())
	{
		window.pollEvents();

		glm::ivec2 currentViewport = window.getViewportSize();

		//Check resize
		if (viewportSize != currentViewport && (currentViewport.x > 0 && currentViewport.y > 0))
		{
			viewportSize = currentViewport;

			destroyFramebuffers(device, framebuffers);
			swapchain.destroy();

			profile = swapchainFactory.createProfileForSurface(renderDevice.getPhysicalDevice(), renderDevice.getSurface(), {}, FullScreenExclusiveMode::DEFAULT, nullptr);
			swapchain = swapchainFactory.createSwapchain(renderDevice.getDevice(), renderDevice.getSurface(), profile, viewportSize.x, viewportSize.y);
			framebuffers = createFramebuffers(device, currentViewport.x, currentViewport.y, swapchain.getImageViews(), renderPass);
		}

		//Render
		uint32_t imageIndex = swapchain.acquireNextImage(imageAvailableSemaphore);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pInheritanceInfo = nullptr;

		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		VkClearValue clearValue = { 0.1f, 0.4f, 0.7f, 1.0f };

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = framebuffers[imageIndex];
		renderPassBeginInfo.renderArea = { { 0, 0 }, { (uint32_t)viewportSize.x, (uint32_t)viewportSize.y } };
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearValue;

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		//Do render stuff

		vkCmdEndRenderPass(commandBuffer);

		VK_CHECK(vkEndCommandBuffer(commandBuffer));

		renderDevice.submit({ commandBuffer }, { { imageAvailableSemaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT } }, { renderCompleteSemaphore }, renderFinishedFence);

		swapchain.present(renderDevice.getQueue(), imageIndex, { renderCompleteSemaphore });

		VK_CHECK(vkWaitForFences(device, 1, &renderFinishedFence, VK_TRUE, UINT64_MAX));
		VK_CHECK(vkResetFences(device, 1, &renderFinishedFence));
		VK_CHECK(vkResetCommandPool(device, commandPool, 0));
	}

	destroyFramebuffers(device, framebuffers);
	vkDestroyRenderPass(device, renderPass, nullptr);

	vkDestroyFence(device, renderFinishedFence, nullptr);
	vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(device, renderCompleteSemaphore, nullptr);

	vkDestroyCommandPool(device, commandPool, nullptr);

	swapchain.destroy();
	renderDevice.destroy();

	window.terminate();

	return 0;
}