#include "ScenePresenter.h"

#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

const char* VERTEX_SHADER = R"(#version 450

layout(location = 0) out vec2 texCoord;

vec2 vertices[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(-1.0, 1.0),

	vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0)
);

void main() {
	texCoord = vertices[gl_VertexIndex];
	gl_Position = vec4(texCoord, 0.0, 1.0);
})";

const char* FRAGMENT_SHADER = R"(#version 450

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D renderTarget;

void main() {
	vec2 uv = 0.5 * texCoord + 0.5;
	vec3 color = texture(renderTarget, uv).rgb;

	outColor = vec4(color, 1.0);
})";

void ScenePresenter::createRenderPass()
{
	VkAttachmentDescription attachments[1] = {};
	attachments[0].format = m_swapchain.getFormat().format;
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

	VK_CHECK(vkCreateRenderPass(m_device->getDevice(), &renderPassCI, nullptr, &m_renderPass));
}

void ScenePresenter::createPipeline()
{
	VkShaderModule vertexShader = m_device->compileShader(VK_SHADER_STAGE_VERTEX_BIT, std::string(VERTEX_SHADER));
	VkShaderModule fragmentShader = m_device->compileShader(VK_SHADER_STAGE_FRAGMENT_BIT, std::string(FRAGMENT_SHADER));

	//Create sampler
	VkSamplerCreateInfo samplerCI = {};
	samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCI.magFilter = VK_FILTER_NEAREST;
	samplerCI.minFilter = VK_FILTER_LINEAR;
	samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCI.mipLodBias = 0.0f;
	samplerCI.anisotropyEnable = VK_FALSE;
	samplerCI.maxAnisotropy = 1.0f;
	samplerCI.compareEnable = VK_FALSE;
	samplerCI.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerCI.minLod = 0.0f;
	samplerCI.maxLod = 1.0f;
	samplerCI.unnormalizedCoordinates = VK_FALSE;

	vkCreateSampler(m_device->getDevice(), &samplerCI, nullptr, &m_sampler);

	//Create descriptor set
	VkDescriptorSetLayoutBinding setLayoutBindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &m_sampler }
	};

	VkDescriptorSetLayoutCreateInfo descSetLayoutCI = {};
	descSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descSetLayoutCI.bindingCount = sizeof(setLayoutBindings) / sizeof(setLayoutBindings[0]);
	descSetLayoutCI.pBindings = setLayoutBindings;

	VK_CHECK(vkCreateDescriptorSetLayout(m_device->getDevice(), &descSetLayoutCI, nullptr, &m_descLayout));

	VkDescriptorPoolSize descPoolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
	};

	uint32_t sizeCount = (uint32_t)(sizeof(descPoolSizes) / sizeof(descPoolSizes[0]));

	VkDescriptorPoolCreateInfo descPoolCI = {};
	descPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolCI.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descPoolCI.poolSizeCount = sizeCount;
	descPoolCI.pPoolSizes = descPoolSizes;
	descPoolCI.maxSets = 1000 * sizeCount;

	VK_CHECK(vkCreateDescriptorPool(m_device->getDevice(), &descPoolCI, nullptr, &m_descriptorPool));

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_descLayout;

	VK_CHECK(vkAllocateDescriptorSets(m_device->getDevice(), &allocInfo, &m_descriptorSet));

	//Create pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = 1;
	pipelineLayoutCI.pSetLayouts = &m_descLayout;

	vkCreatePipelineLayout(m_device->getDevice(), &pipelineLayoutCI, nullptr, &m_pipelineLayout);

	//Create pipeline
	VkViewport emptyViewport = { 0, 0, 0, 0, 0, 0 };
	VkRect2D emptyScissor = { { 0, 0 }, { 0, 0 } };

	VkPipelineShaderStageCreateInfo stages[2] = {};

	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vertexShader;
	stages[0].pName = "main";
	stages[0].pSpecializationInfo = nullptr;

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fragmentShader;
	stages[1].pName = "main";
	stages[1].pSpecializationInfo = nullptr;

	VkPipelineVertexInputStateCreateInfo vertexInputStateCI = {};
	vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCI.vertexBindingDescriptionCount = 0;
	vertexInputStateCI.pVertexBindingDescriptions = nullptr;
	vertexInputStateCI.vertexAttributeDescriptionCount = 0;
	vertexInputStateCI.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = {};
	inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCI.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCI = {};
	viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCI.viewportCount = 1;
	viewportStateCI.pViewports = &emptyViewport;
	viewportStateCI.scissorCount = 1;
	viewportStateCI.pScissors = &emptyScissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = {};
	rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCI.depthClampEnable = VK_FALSE;
	rasterizationStateCI.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
	rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCI.depthBiasEnable = VK_FALSE;
	rasterizationStateCI.depthBiasConstantFactor = 0.0f;
	rasterizationStateCI.depthBiasClamp = 0.0f;
	rasterizationStateCI.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCI.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisamplingCI = {};
	multisamplingCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisamplingCI.sampleShadingEnable = VK_FALSE;
	multisamplingCI.minSampleShading = 1.0f;
	multisamplingCI.pSampleMask = nullptr;
	multisamplingCI.alphaToCoverageEnable = VK_FALSE;
	multisamplingCI.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachment = {};
	blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachment.blendEnable = VK_TRUE;
	blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo blendStateCI = {};
	blendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendStateCI.logicOpEnable = VK_FALSE;
	blendStateCI.logicOp = VK_LOGIC_OP_COPY;
	blendStateCI.attachmentCount = 1;
	blendStateCI.pAttachments = &blendAttachment;
	blendStateCI.blendConstants[0] = 0.0f;
	blendStateCI.blendConstants[1] = 0.0f;
	blendStateCI.blendConstants[2] = 0.0f;
	blendStateCI.blendConstants[3] = 0.0f;

	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicStateCI = {};
	dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCI.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicStateCI.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	createInfo.stageCount = 2;
	createInfo.pStages = stages;
	createInfo.pVertexInputState = &vertexInputStateCI;
	createInfo.pInputAssemblyState = &inputAssemblyStateCI;
	createInfo.pTessellationState = nullptr;
	createInfo.pViewportState = &viewportStateCI;
	createInfo.pRasterizationState = &rasterizationStateCI;
	createInfo.pMultisampleState = &multisamplingCI;
	createInfo.pDepthStencilState = nullptr;
	createInfo.pColorBlendState = &blendStateCI;
	createInfo.pDynamicState = &dynamicStateCI;
	createInfo.layout = m_pipelineLayout;
	createInfo.renderPass = m_renderPass;
	createInfo.subpass = 0;

	VK_CHECK(vkCreateGraphicsPipelines(m_device->getDevice(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_pipeline));

	vkDestroyShaderModule(m_device->getDevice(), vertexShader, nullptr);
	vkDestroyShaderModule(m_device->getDevice(), fragmentShader, nullptr);
}

void ScenePresenter::createSwapchain()
{
	m_surfaceProfile = m_swapchainFactory.createProfileForSurface(m_device->getPhysicalDevice(), m_device->getSurface(), {}, FullScreenExclusiveMode::DEFAULT);
	m_swapchain = m_swapchainFactory.createSwapchain(m_device->getDevice(), m_device->getSurface(), m_surfaceProfile, m_width, m_height);
}

void ScenePresenter::destroySwapchain()
{
	m_swapchain.destroy();
}

void ScenePresenter::createFramebuffers()
{
	m_framebuffers.resize(m_swapchain.getImageViews().size());

	for (size_t i = 0; i < m_swapchain.getImageViews().size(); ++i)
	{
		VkImageView imageView = m_swapchain.getImageViews()[i];

		VkFramebufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = m_renderPass;
		createInfo.attachmentCount = 1;
		createInfo.pAttachments = &imageView;
		createInfo.width = m_width;
		createInfo.height = m_height;
		createInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(m_device->getDevice(), &createInfo, nullptr, &m_framebuffers[i]));
	}
}

void ScenePresenter::createCommandBuffers()
{
	VkDevice device = m_device->getDevice();

	m_commandPool = m_device->createCommandPool();

	//Allocate command buffers
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.commandBufferCount = 1;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &m_commandBuffer));

	//Create synchronization primitives
	VkSemaphoreCreateInfo semaphoreCI = {};
	semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VK_CHECK(vkCreateSemaphore(device, &semaphoreCI, nullptr, &m_imageAvailableSemaphore));
	VK_CHECK(vkCreateSemaphore(device, &semaphoreCI, nullptr, &m_renderCompleteSemaphore));

	VkFenceCreateInfo fenceCI = {};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	VK_CHECK(vkCreateFence(device, &fenceCI, nullptr, &m_renderFinishedFence));

	//Create query pool
	VkQueryPoolCreateInfo queryPoolCI = {};
	queryPoolCI.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	queryPoolCI.queryType = VK_QUERY_TYPE_TIMESTAMP;
	queryPoolCI.queryCount = 2;
	queryPoolCI.pipelineStatistics = 0;
	
	VK_CHECK(vkCreateQueryPool(device, &queryPoolCI, nullptr, &m_queryPool));
}

void ScenePresenter::destroyCommandBuffers()
{
	VkDevice device = m_device->getDevice();

	//Destroy command pool
	vkDestroyCommandPool(device, m_commandPool, nullptr);

	//Destroy synchronization primitives
	vkDestroyFence(device, m_renderFinishedFence, nullptr);
	vkDestroySemaphore(device, m_imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(device, m_renderCompleteSemaphore, nullptr);

	vkDestroyQueryPool(device, m_queryPool, nullptr);
}

void ScenePresenter::destroyFramebuffers()
{
	for (size_t i = 0; i < m_framebuffers.size(); ++i)
	{
		vkDestroyFramebuffer(m_device->getDevice(), m_framebuffers[i], nullptr);
	}
	
	m_framebuffers.clear();
}

PFN_vkVoidFunction imguiLoadVulkanFunc(const char* functionName, void* userData)
{
	const RenderDevice* device = (const RenderDevice*)userData;

	PFN_vkVoidFunction func = vkGetDeviceProcAddr(device->getDevice(), functionName);
	return func ? func : vkGetInstanceProcAddr(device->getInstance(), functionName);
}

void ScenePresenter::initImGui(const Window& window)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();

	if (!ImGui_ImplVulkan_LoadFunctions(&imguiLoadVulkanFunc, (void*)m_device))
	{
		FATAL_ERROR("Failed to create ImGui");
	}

	ImGui_ImplGlfw_InitForVulkan(window.getHandle(), true);
	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = m_device->getInstance();
	initInfo.PhysicalDevice = m_device->getPhysicalDevice();
	initInfo.Device = m_device->getDevice();
	initInfo.QueueFamily = m_device->getQueueFamily();
	initInfo.Queue = m_device->getQueue();
	initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = m_descriptorPool;
	initInfo.Subpass = 0;
	initInfo.MinImageCount = m_surfaceProfile.capabilties.minImageCount;
	initInfo.ImageCount = (uint32_t)m_swapchain.getImageViews().size();
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.Allocator = nullptr;

	if (!ImGui_ImplVulkan_Init(&initInfo, m_renderPass))
	{
		FATAL_ERROR("Failed to create ImGui");
	}

	m_device->executeCommands(1, [](VkCommandBuffer* commandBuffers)
	{
		ImGui_ImplVulkan_CreateFontsTexture(commandBuffers[0]);
	});

	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void ScenePresenter::destroyImGui()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void ScenePresenter::init(const RenderDevice* device, const Window& window, int initialWidth, int initialHeight)
{
	m_device = device;
	m_width = initialWidth;
	m_height = initialHeight;

	//Get timestap period
	VkPhysicalDeviceProperties properties;
	m_device->getPhysicalDevicePropertes(&properties, nullptr);

	m_timestampPeriod = properties.limits.timestampPeriod;

	//Initialize scene presenter 
	createSwapchain();
	createRenderPass();
	createFramebuffers();
	createPipeline();
	createCommandBuffers();

	initImGui(window);
}

void ScenePresenter::destroy()
{
	destroyImGui();

	vkDestroySampler(m_device->getDevice(), m_sampler, nullptr);

	vkDestroyDescriptorSetLayout(m_device->getDevice(), m_descLayout, nullptr);
	vkDestroyDescriptorPool(m_device->getDevice(), m_descriptorPool, nullptr);

	vkDestroyPipelineLayout(m_device->getDevice(), m_pipelineLayout, nullptr);
	vkDestroyPipeline(m_device->getDevice(), m_pipeline, nullptr);
	vkDestroyRenderPass(m_device->getDevice(), m_renderPass, nullptr);

	destroyCommandBuffers();
	destroyFramebuffers();
	destroySwapchain();
}

void ScenePresenter::resize(int width, int height)
{
	m_width = width;
	m_height = height;

	destroySwapchain();
	destroyFramebuffers();

	createSwapchain();
	createFramebuffers();
}

VkCommandBuffer ScenePresenter::beginFrame()
{
	//Get next image index
	m_imageIndex = m_swapchain.acquireNextImage(m_imageAvailableSemaphore);

	//Begin command buffer
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pInheritanceInfo = nullptr;

	VK_CHECK(vkBeginCommandBuffer(m_commandBuffer, &beginInfo));

	vkCmdWriteTimestamp(m_commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPool, 0);

	return m_commandBuffer;
}

void ScenePresenter::endFrame(const RaytracingPipeline* pipeline, VkRect2D renderArea, ImageState prevImageState)
{
	vkCmdWriteTimestamp(m_commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, 1);

	if (pipeline)
	{
		//Update pipeline descriptor set
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = pipeline->getRenderTarget().imageView;

		VkWriteDescriptorSet setWrite = {};
		setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		setWrite.dstSet = m_descriptorSet;
		setWrite.dstBinding = 0;
		setWrite.descriptorCount = 1;
		setWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		setWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(m_device->getDevice(), 1, &setWrite, 0, nullptr);

		//Insert barrier to waits for output image to be rendered
		VkImageMemoryBarrier imageBarrier = {};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier.srcAccessMask = prevImageState.accessFlags;
		imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageBarrier.oldLayout = prevImageState.layout;
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.image = pipeline->getRenderTarget().image;
		imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.layerCount = 1;
		imageBarrier.subresourceRange.baseMipLevel = 0;
		imageBarrier.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(m_commandBuffer, prevImageState.stageMask, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
											  0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
	}

	VkClearValue clearValue = { 0.1f, 0.4f, 0.7f, 1.0f };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_renderPass;
	renderPassBeginInfo.framebuffer = m_framebuffers[m_imageIndex];
	renderPassBeginInfo.renderArea = renderArea;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearValue;

	vkCmdBeginRenderPass(m_commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	//Record drawing commands
	VkViewport viewport = { 0, 0, (float)m_width, (float)m_height, 0, 1 };
	VkRect2D scissor = { { 0, 0 }, { (uint32_t)m_width, (uint32_t)m_height } };

	vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

	if (pipeline)
	{
		vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
		vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

		vkCmdDraw(m_commandBuffer, 6, 1, 0, 0);
	}

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffer);

	vkCmdEndRenderPass(m_commandBuffer);

	if (pipeline)
	{
		VkImageMemoryBarrier imageBarrier = {};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageBarrier.dstAccessMask = 0;
		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarrier.newLayout = prevImageState.layout;
		imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.image = pipeline->getRenderTarget().image;
		imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.layerCount = 1;
		imageBarrier.subresourceRange.baseMipLevel = 0;
		imageBarrier.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
											  0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
	}

	//Finish command buffer
	VK_CHECK(vkEndCommandBuffer(m_commandBuffer));

	vkResetQueryPool(m_device->getDevice(), m_queryPool, 0, 2);

	m_device->submit({ m_commandBuffer }, { { m_imageAvailableSemaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT } }, { m_renderCompleteSemaphore }, m_renderFinishedFence);

	m_swapchain.present(m_device->getQueue(), m_imageIndex, { m_renderCompleteSemaphore });

	VK_CHECK(vkWaitForFences(m_device->getDevice(), 1, &m_renderFinishedFence, VK_TRUE, UINT64_MAX));
	VK_CHECK(vkResetFences(m_device->getDevice(), 1, &m_renderFinishedFence));
	VK_CHECK(vkResetCommandPool(m_device->getDevice(), m_commandPool, 0));

	uint64_t timestamps[2];
	VK_CHECK(vkGetQueryPoolResults(m_device->getDevice(), m_queryPool, 0, 2, sizeof(timestamps), timestamps, sizeof(timestamps[0]), VK_QUERY_RESULT_64_BIT));

	m_renderTime = (double)(timestamps[1] - timestamps[0]) / 1000000000.0 * m_timestampPeriod;
}