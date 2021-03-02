#include "ScenePresenter.h"

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

void main() {
	outColor = vec4(0.5 * texCoord + 0.5, 0.5, 1.0);
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

	//Create descriptor set layout
	VkDescriptorSetLayoutBinding setLayoutBindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }
	};

	//Create pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = 0;
	pipelineLayoutCI.pSetLayouts = nullptr;

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

void ScenePresenter::destroyFramebuffers()
{
	for (size_t i = 0; i < m_framebuffers.size(); ++i)
	{
		vkDestroyFramebuffer(m_device->getDevice(), m_framebuffers[i], nullptr);
	}
	
	m_framebuffers.clear();
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

void ScenePresenter::init(const RenderDevice* device, int initialWidth, int initialHeight)
{
	m_device = device;
	m_width = initialWidth;
	m_height = initialHeight;

	createSwapchain();
	createRenderPass();
	createFramebuffers();
	createPipeline();
}

void ScenePresenter::destroy()
{
	vkDestroyPipelineLayout(m_device->getDevice(), m_pipelineLayout, nullptr);
	vkDestroyPipeline(m_device->getDevice(), m_pipeline, nullptr);
	vkDestroyRenderPass(m_device->getDevice(), m_renderPass, nullptr);

	destroyFramebuffers();
	destroySwapchain();
}

void ScenePresenter::beginRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex, VkRect2D renderArea) const
{
	VkClearValue clearValue = { 0.1f, 0.4f, 0.7f, 1.0f };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_renderPass;
	renderPassBeginInfo.framebuffer = m_framebuffers[imageIndex];
	renderPassBeginInfo.renderArea = renderArea;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearValue;

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void ScenePresenter::endRenderPass(VkCommandBuffer commandBuffer) const
{
	vkCmdEndRenderPass(commandBuffer);
}

void ScenePresenter::showRender(VkCommandBuffer commandBuffer) const
{
	VkViewport viewport = { 0, 0, (float)m_width, (float)m_height, 0, 1 };
	VkRect2D scissor = { { 0, 0 }, { (uint32_t)m_width, (uint32_t)m_height } };

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdDraw(commandBuffer, 6, 1, 0, 0);
}