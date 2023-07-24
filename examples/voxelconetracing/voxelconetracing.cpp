/*
* Vulkan Example - Deferred shading with shadows from multiple light sources using geometry shader instancing
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanFrameBuffer.hpp"
#include "VulkanglTFModel.h"

/******************************/
#include "core/VulkanQueue.h"

#include "core/Vertex.h"
#include "core/PostProcess.h"
#include "core/Shadow.h"
#include "core/Voxelization.h"

#include "assets/Material.h"
#include "assets/Geometry.h"
#include "assets/AssetDatabase.h"

#include "actors/Object.h"
#include "actors/Light.h"
#include <sstream>


//#include "core/VulkanApp.h"


#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

// Shadowmap properties
#if defined(__ANDROID__)
#define SHADOWMAP_DIM 1024
#else
#define SHADOWMAP_DIM 2048
#endif

#if defined(__ANDROID__)
// Use max. screen dimension as deferred framebuffer size
#define FB_DIM std::max(width,height)
#else
#define FB_DIM 2048
#endif

// Must match the LIGHT_COUNT define in the shadow and deferred shaders
#define LIGHT_COUNT 3


class VulkanExample : public VulkanExampleBase
{
public:

	float zNear = 0.1f;
	float zFar = 100.0f;
	int32_t debugDisplayTarget = 0;

	float mainLightAngle = 1.0f;
	std::vector<DirectionalLight> directionLights;

	StandardShadow standardShadow;
	Voxelization voxelizator;

	PostProcess* sceneStage;  //for FrameRender
	PostProcess* theLastPostProcess;
	std::vector<PostProcess*> postProcessStages;

	/*
	VkQueue objectDrawQueue, TagQueue, AllocationQueue, MipmapQueue;
	VkQueue lightingQueue, postProcessQueue, presentQueue;
	*/

	VoxelConetracingMaterial* voxelConetracingMaterial;
	LightingMaterial* lightingMaterial;

	HDRHighlightMaterial* hdrHighlightMaterial;

	BlurMaterial* HBMaterial;
	BlurMaterial* VBMaterial;

	BlurMaterial* HBMaterial2;
	BlurMaterial* VBMaterial2;

	ComputeBlurMaterial* compHBMaterial;
	ComputeBlurMaterial* compVBMaterial;

	ComputeBlurMaterial* compHBMaterial2;
	ComputeBlurMaterial* compVBMaterial2;

	singleTriangular* offScreenPlane;
	singleTriangular* offScreenPlaneforPostProcess;
	singleQuadral* debugDisplayPlane;

	std::vector<DebugDisplayMaterial*> debugDisplayMaterials;
	LastPostProcessgMaterial* lastPostProcessMaterial;
	//VoxelRenderMaterial* voxelRenderMaterial;
	FinalRenderingMaterial* frameBufferMaterial;

	VkRenderPass deferredRenderPass;
#if 0
	VkCommandPool deferredCommandPool;
#endif
	VkCommandBuffer deferredCommandBuffer;
	VkFramebuffer deferredFrameBuffer;

	// TODO: use std::vector<vks::Texture2D> later
	std::vector<VkImage> gBufferImages;
	std::vector<VkImageView> gBufferImageViews;
	std::vector<VkDeviceMemory> gBufferImageMemories;

	// TODO: use vks::Texture2D later
	VkImage sceneImage;
	VkImageView sceneImageView;
	VkDeviceMemory sceneImageMemories;

	/**************** Extra Semaphores ****************/
	VkSemaphore objectDrawCompleteSemaphore = VK_NULL_HANDLE;











	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Voxel Cone Tracing GI";
		name = "voxelConeTracingGI";
		
		// Camera
		camera.type = Camera::CameraType::firstperson;
#if defined(__ANDROID__)
		camera.movementSpeed = 2.5f;
#else
		camera.movementSpeed = 5.0f;
		camera.rotationSpeed = 0.25f;
#endif
		camera.flipY = true;
		camera.position = { 0.0f, -3.0f, 5.0f };
		camera.setRotation({ 0.0f, 45.0f, 0.0f });
		camera.setPerspective(45.0f, (float)width / (float)height, zNear, zFar);
		timerSpeed *= 0.25f;

		paused = true;
	}

	~VulkanExample()
	{
		// Frame buffers
		if (frameBuffers.deferred)
		{
			delete frameBuffers.deferred;
		}
		if (frameBuffers.shadow)
		{
			delete frameBuffers.shadow;
		}

		vkDestroyPipeline(device, pipelines.deferred, nullptr);
		vkDestroyPipeline(device, pipelines.offscreen, nullptr);
		vkDestroyPipeline(device, pipelines.shadowpass, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		// Uniform buffers
		uniformBuffers.composition.destroy();
		uniformBuffers.offscreen.destroy();
		uniformBuffers.shadowGeometryShader.destroy();

		// Textures
		textures.model.colorMap.destroy();
		textures.model.normalMap.destroy();
		textures.background.colorMap.destroy();
		textures.background.normalMap.destroy();

		vkDestroySemaphore(device, offscreenSemaphore, nullptr);
	}







	// GOOD
	// Enable physical device features required for this example
	virtual void getEnabledFeatures() override
	{
		// Geometry shader support is required for voxelization
		if (deviceFeatures.geometryShader) {
			enabledFeatures.geometryShader = VK_TRUE;
		}
		else {
			vks::tools::exitFatal("Selected GPU does not support geometry shaders!", VK_ERROR_FEATURE_NOT_PRESENT);
		}
		// Enable anisotropic filtering if supported
		if (deviceFeatures.samplerAnisotropy) {
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		}
		// Enable texture compression
		if (deviceFeatures.textureCompressionBC) {
			enabledFeatures.textureCompressionBC = VK_TRUE;
		}
		else if (deviceFeatures.textureCompressionASTC_LDR) {
			enabledFeatures.textureCompressionASTC_LDR = VK_TRUE;
		}
		else if (deviceFeatures.textureCompressionETC2) {
			enabledFeatures.textureCompressionETC2 = VK_TRUE;
		}
	}

	// GOOD
	DirectionalLight initLight(glm::vec3 pos, glm::vec3 target, glm::vec3 color)
	{
		DirectionalLight light;
		light.lightInfo.lightPosition = glm::vec4(pos, 1.0);
		light.lightInfo.focusPosition = glm::vec4(target, 1.0);
		light.lightInfo.lightColor = glm::vec4(color, 1.0);
		return light;
	}

	// GOOD
	void initLights()
	{
		directionLights.push_back(initLight({ 1.0, 1.0, 1.0 }, { -0.5, 14.5, -0.4 }, { -0.5, 14.618034, -0.4 }));  // main light
		swingMainLight();
	}

	// GOOD
	void swingMainLight()
	{
		SwingXAxisDirectionalLight(directionLights[0].lightInfo, 1.0f, mainLightAngle, 0.5f);
	}

	// GOOD
	void updateDrawMode()
	{
		void* data;
		vkMapMemory(device, lightingMaterial->optionBufferMemory, 0, sizeof(uint32_t), 0, &data);
		memcpy(data, &drawMode, sizeof(uint32_t));
		vkUnmapMemory(device, lightingMaterial->optionBufferMemory);
	}

	// GOOD
	void switchTheLastPostProcess(unsigned int from, unsigned int to)
	{
		if (to >= postProcessStages.size())
			return;

		if (theLastPostProcess == postProcessStages[to])
			theLastPostProcess = postProcessStages[from];
		else
			theLastPostProcess = postProcessStages[to];
	}











	// Prepare a layered shadow map with each layer containing depth from a light's point of view
	// The shadow mapping pass uses geometry shader instancing to output the scene from the different
	// light sources' point of view to the layers of the depth attachment in one single pass
	void shadowSetup()
	{
		frameBuffers.shadow = new vks::Framebuffer(vulkanDevice);

		frameBuffers.shadow->width = SHADOWMAP_DIM;
		frameBuffers.shadow->height = SHADOWMAP_DIM;

		// Find a suitable depth format
		VkFormat shadowMapFormat;
		VkBool32 validShadowMapFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &shadowMapFormat);
		assert(validShadowMapFormat);

		// Create a layered depth attachment for rendering the depth maps from the lights' point of view
		// Each layer corresponds to one of the lights
		// The actual output to the separate layers is done in the geometry shader using shader instancing
		// We will pass the matrices of the lights to the GS that selects the layer by the current invocation
		vks::AttachmentCreateInfo attachmentInfo = {};
		attachmentInfo.format = shadowMapFormat;
		attachmentInfo.width = SHADOWMAP_DIM;
		attachmentInfo.height = SHADOWMAP_DIM;
		attachmentInfo.layerCount = LIGHT_COUNT;
		attachmentInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		frameBuffers.shadow->addAttachment(attachmentInfo);

		// Create sampler to sample from to depth attachment
		// Used to sample in the fragment shader for shadowed rendering
		VK_CHECK_RESULT(frameBuffers.shadow->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));

		// Create default renderpass for the framebuffer
		VK_CHECK_RESULT(frameBuffers.shadow->createRenderPass());
	}

	// Prepare the framebuffer for offscreen rendering with multiple attachments used as render targets inside the fragment shaders
	void deferredSetup()
	{
		frameBuffers.deferred = new vks::Framebuffer(vulkanDevice);

		frameBuffers.deferred->width = FB_DIM;
		frameBuffers.deferred->height = FB_DIM;

		// Four attachments (3 color, 1 depth)
		vks::AttachmentCreateInfo attachmentInfo = {};
		attachmentInfo.width = FB_DIM;
		attachmentInfo.height = FB_DIM;
		attachmentInfo.layerCount = 1;
		attachmentInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// Color attachments
		// Attachment 0: (World space) Positions
		attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		frameBuffers.deferred->addAttachment(attachmentInfo);

		// Attachment 1: (World space) Normals
		attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		frameBuffers.deferred->addAttachment(attachmentInfo);

		// Attachment 2: Albedo (color)
		attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		frameBuffers.deferred->addAttachment(attachmentInfo);

		// Depth attachment
		// Find a suitable depth format
		VkFormat attDepthFormat;
		VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &attDepthFormat);
		assert(validDepthFormat);

		attachmentInfo.format = attDepthFormat;
		attachmentInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		frameBuffers.deferred->addAttachment(attachmentInfo);

		// Create sampler to sample from the color attachments
		VK_CHECK_RESULT(frameBuffers.deferred->createSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));

		// Create default renderpass for the framebuffer
		VK_CHECK_RESULT(frameBuffers.deferred->createRenderPass());
	}

	// Put render commands for the scene into the given command buffer
	void renderScene(VkCommandBuffer cmdBuffer, bool shadow)
	{
		// Background
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, shadow ? &descriptorSets.shadow : &descriptorSets.background, 0, NULL);
		models.background.draw(cmdBuffer);

		// Objects
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, shadow ? &descriptorSets.shadow : &descriptorSets.model, 0, NULL);
		models.model.bindBuffers(cmdBuffer);
		vkCmdDrawIndexed(cmdBuffer, models.model.indices.count, 3, 0, 0, 0);
	}

	// Build a secondary command buffer for rendering the scene values to the offscreen frame buffer attachments
	void buildDeferredCommandBuffer()
	{
		if (commandBuffers.deferred == VK_NULL_HANDLE)
		{
			commandBuffers.deferred = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		}

		// Create a semaphore used to synchronize offscreen rendering and usage
		VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
		VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &offscreenSemaphore));

		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		std::array<VkClearValue, 4> clearValues = {};
		VkViewport viewport;
		VkRect2D scissor;

		// First pass: Shadow map generation
		// -------------------------------------------------------------------------------------------------------

		clearValues[0].depthStencil = { 1.0f, 0 };

		renderPassBeginInfo.renderPass = frameBuffers.shadow->renderPass;
		renderPassBeginInfo.framebuffer = frameBuffers.shadow->framebuffer;
		renderPassBeginInfo.renderArea.extent.width = frameBuffers.shadow->width;
		renderPassBeginInfo.renderArea.extent.height = frameBuffers.shadow->height;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues.data();

		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffers.deferred, &cmdBufInfo));

		viewport = vks::initializers::viewport((float)frameBuffers.shadow->width, (float)frameBuffers.shadow->height, 0.0f, 1.0f);
		vkCmdSetViewport(commandBuffers.deferred, 0, 1, &viewport);

		scissor = vks::initializers::rect2D(frameBuffers.shadow->width, frameBuffers.shadow->height, 0, 0);
		vkCmdSetScissor(commandBuffers.deferred, 0, 1, &scissor);

		// Set depth bias (aka "Polygon offset")
		vkCmdSetDepthBias(
			commandBuffers.deferred,
			depthBiasConstant,
			0.0f,
			depthBiasSlope);

		vkCmdBeginRenderPass(commandBuffers.deferred, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffers.deferred, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.shadowpass);
		renderScene(commandBuffers.deferred, true);  // put command into the deferred command buffer
		vkCmdEndRenderPass(commandBuffers.deferred);

		// Second pass: Deferred calculations
		// -------------------------------------------------------------------------------------------------------

		// Clear values for all attachments written in the fragment shader
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[3].depthStencil = { 1.0f, 0 };

		renderPassBeginInfo.renderPass = frameBuffers.deferred->renderPass;
		renderPassBeginInfo.framebuffer = frameBuffers.deferred->framebuffer;
		renderPassBeginInfo.renderArea.extent.width = frameBuffers.deferred->width;
		renderPassBeginInfo.renderArea.extent.height = frameBuffers.deferred->height;
		renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassBeginInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffers.deferred, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		viewport = vks::initializers::viewport((float)frameBuffers.deferred->width, (float)frameBuffers.deferred->height, 0.0f, 1.0f);
		vkCmdSetViewport(commandBuffers.deferred, 0, 1, &viewport);

		scissor = vks::initializers::rect2D(frameBuffers.deferred->width, frameBuffers.deferred->height, 0, 0);
		vkCmdSetScissor(commandBuffers.deferred, 0, 1, &scissor);

		vkCmdBindPipeline(commandBuffers.deferred, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.offscreen);
		renderScene(commandBuffers.deferred, false);
		vkCmdEndRenderPass(commandBuffers.deferred);

		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffers.deferred));
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		models.model.loadFromFile(getAssetPath() + "models/armor/armor.gltf", vulkanDevice, queue, glTFLoadingFlags);
		models.background.loadFromFile(getAssetPath() + "models/deferred_box.gltf", vulkanDevice, queue, glTFLoadingFlags);
		textures.model.colorMap.loadFromFile(getAssetPath() + "models/armor/colormap_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		textures.model.normalMap.loadFromFile(getAssetPath() + "models/armor/normalmap_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		textures.background.colorMap.loadFromFile(getAssetPath() + "textures/stonefloor02_color_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		textures.background.normalMap.loadFromFile(getAssetPath() + "textures/stonefloor02_normal_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);

		// TODO: VCT
		
	}

	void buildCommandBuffers() override  // record draw cmd buffers commands
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = VulkanExampleBase::frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

			// Final composition as full screen quad
			// Note: Also used for debug display if debugDisplayTarget > 0
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.deferred);
			vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();

		// Final fullscreen composition pass pipeline
		{
			rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
			shaderStages[0] = loadShader(getShadersPath() + "voxelconetracing/deferred.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1] = loadShader(getShadersPath() + "voxelconetracing/deferred.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
			// Empty vertex input state, vertices are generated by the vertex shader
			VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
			pipelineCI.pVertexInputState = &emptyInputState;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.deferred));
		}

		// Vertex input state from glTF model for pipeline rendering models
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Tangent });
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

		// Offscreen pipeline
		{
			// Separate render pass
			pipelineCI.renderPass = frameBuffers.deferred->renderPass;

			// Blend attachment states required for all color attachments
			// This is important, as color write mask will otherwise be 0x0 and you
			// won't see anything rendered to the attachment
			std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates =
			{
				vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
				vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
				vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE)
			};
			colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
			colorBlendState.pAttachments = blendAttachmentStates.data();

			shaderStages[0] = loadShader(getShadersPath() + "voxelconetracing/mrt.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1] = loadShader(getShadersPath() + "voxelconetracing/mrt.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.offscreen));
		}

		// Shadow mapping pipeline
		{
			// The shadow mapping pipeline uses geometry shader instancing (invocations layout modifier) to output
			// shadow maps for multiple lights sources into the different shadow map layers in one single render pass
			std::array<VkPipelineShaderStageCreateInfo, 2> shadowStages;
			shadowStages[0] = loadShader(getShadersPath() + "voxelconetracing/shadow.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			shadowStages[1] = loadShader(getShadersPath() + "voxelconetracing/shadow.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

			pipelineCI.pStages = shadowStages.data();
			pipelineCI.stageCount = static_cast<uint32_t>(shadowStages.size());

			// Shadow pass doesn't use any color attachments
			colorBlendState.attachmentCount = 0;
			colorBlendState.pAttachments = nullptr;
			// Cull front faces
			rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
			depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			// Enable depth bias
			rasterizationState.depthBiasEnable = VK_TRUE;
			// Add depth bias to dynamic state, so we can change it at runtime
			dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
			dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
			// Reset blend attachment state
			pipelineCI.renderPass = frameBuffers.shadow->renderPass;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.shadowpass));
		}
	}








	// GOOD
	void createGbuffers()
	{
		gBufferImages.resize(NUM_GBUFFERS);
		gBufferImageMemories.resize(NUM_GBUFFERS);

		for (uint32_t i = 0; i < gBufferImages.size(); i++)
		{
			if (i == NORMAL_COLOR)
			{
				createImage(width, height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gBufferImages[i], gBufferImageMemories[i]);
			}
			else
			{
				createImage(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gBufferImages[i], gBufferImageMemories[i]);
			}
		}
	}

	// GOOD
	void createSceneBuffer()
	{
		createImage(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sceneImage, sceneImageMemories);
	}

	// GOOD
	void createDeferredFramebuffer()
	{
		std::array<VkImageView, NUM_GBUFFERS + 1> attachments{
			gBufferImageViews[BASIC_COLOR], gBufferImageViews[SPECULAR_COLOR], 
			gBufferImageViews[NORMAL_COLOR], gBufferImageViews[EMISSIVE_COLOR], depthStencil.view /*depthImageView*/
		};

		VkFramebufferCreateInfo deferredFramebufferCI = vks::initializers::framebufferCreateInfo();
		deferredFramebufferCI.renderPass = deferredRenderPass;
		deferredFramebufferCI.pAttachments = attachments.data();
		deferredFramebufferCI.attachmentCount = static_cast<uint32_t>(attachments.size());
		deferredFramebufferCI.width = width;
		deferredFramebufferCI.height = height;
		deferredFramebufferCI.layers = 1;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &deferredFramebufferCI, nullptr, &deferredFrameBuffer));
	}

	// GOOD
	void createDeferredRenderPass()
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription specColorAttachment = {};
		specColorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		specColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		specColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		specColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		specColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		specColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		specColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		specColorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription normalColorAttachment = {};
		normalColorAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		normalColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		normalColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		normalColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		normalColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		normalColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		normalColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		normalColorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription emissiveColorAttachment = {};
		emissiveColorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		emissiveColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		emissiveColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		emissiveColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		emissiveColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		emissiveColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		emissiveColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		emissiveColorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment = {};
		{
			VkFormat attDepthFormat;
			assert(vks::tools::getSupportedDepthFormat(physicalDevice, &attDepthFormat) && "no supported depth format");
			depthAttachment.format = attDepthFormat;
		}
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // We will read from depth, so it's important to store the depth attachment results
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; // Attachment will be transitioned to shader read at render pass end


		//Subpasses and attachment references
		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference specColorAttachmentRef = {};
		specColorAttachmentRef.attachment = 1;
		specColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference normalColorAttachmentRef = {};
		normalColorAttachmentRef.attachment = 2;
		normalColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference emissiveColorAttachmentRef = {};
		emissiveColorAttachmentRef.attachment = 3;
		emissiveColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef = {};
		depthAttachmentRef.attachment = 4;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		std::array<VkAttachmentReference, 4> gBuffersAttachmentRef = { colorAttachmentRef, specColorAttachmentRef, normalColorAttachmentRef, emissiveColorAttachmentRef };

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = static_cast<uint32_t>(gBuffersAttachmentRef.size());
		subpass.pColorAttachments = gBuffersAttachmentRef.data();
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		/*
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		*/

		// Use subpass dependencies for attachment layout transitions
		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Create render pass
		std::array<VkAttachmentDescription, 5> attachments = { colorAttachment, specColorAttachment, normalColorAttachment, emissiveColorAttachment, depthAttachment };
		VkRenderPassCreateInfo renderPassInfo = vks::initializers::renderPassCreateInfo();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderPassInfo.pDependencies = dependencies.data();
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &deferredRenderPass));
	}

	// GOOD
	void updateUniformBuffers(float deltaTime)
	{
		if (bRotateMainLight)
		{
			mainLightAngle += static_cast<float>(deltaTime) * 0.2f;
			swingMainLight();
		}

		SetDirectionLightMatrices(directionLights[0], 19.0f, 5.0f, 0.0f, 20.0f);

		UniformBufferObject ubo = {};
		ubo.modelMat = glm::mat4(1.0);
		ubo.viewMat = camera.matrices.view;
		ubo.projMat = camera.matrices.perspective;
		ubo.viewProjMat = camera.matrices.viewProj;
		ubo.InvViewProjMat = camera.matrices.invViewProj;
		ubo.modelViewProjMat = ubo.viewProjMat;
		ubo.cameraWorldPos = camera.position;
		ubo.InvTransposeMat = ubo.modelMat;

		ShadowUniformBuffer subo = {};
		subo.viewProjMat = directionLights[0].projMat * directionLights[0].viewMat;
		subo.invViewProjMat = glm::inverse(subo.viewProjMat);

		void* shadowdata;
		vkMapMemory(device, lightingMaterial->shadowConstantBufferMemory, 0, sizeof(ShadowUniformBuffer), 0, &shadowdata);
		memcpy(shadowdata, &subo, sizeof(ShadowUniformBuffer));
		vkUnmapMemory(device, lightingMaterial->shadowConstantBufferMemory);

		for (size_t i = 0; i < objectManager.size(); i++)
		{
			Object* thisObject = objectManager[i];

			if (thisObject->bRoll)
				thisObject->UpdateOrbit(deltaTime * thisObject->rollSpeed, 0.0f, 0.0f);

			ubo.modelMat = thisObject->modelMat;
			ubo.modelViewProjMat = ubo.viewProjMat * thisObject->modelMat;

			glm::mat4 A = ubo.modelMat;
			A[3] = glm::vec4(0, 0, 0, 1);
			ubo.InvTransposeMat = glm::transpose(glm::inverse(A));


			//shadow
			{
				void* data;
				//vkMapMemory(device, thisObject->shadowMaterial->ShadowConstantBufferMemory, 0, sizeof(ShadowUniformBuffer), 0, &data);
				//memcpy(data, &subo, sizeof(ShadowUniformBuffer));
				//vkUnmapMemory(device, thisObject->shadowMaterial->ShadowConstantBufferMemory);


				ObjectUniformBuffer obu = {};
				obu.modelMat = thisObject->modelMat;

				vkMapMemory(device, thisObject->shadowMaterial->objectUniformMemory, 0, sizeof(ObjectUniformBuffer), 0, &data);
				memcpy(data, &obu, sizeof(ObjectUniformBuffer));
				vkUnmapMemory(device, thisObject->shadowMaterial->objectUniformMemory);


			}

			for (size_t k = 0; k < thisObject->materials.size(); k++)
			{
				void* data;
				vkMapMemory(device, thisObject->materials[k]->uniformBufferMemory, 0, sizeof(UniformBufferObject), 0, &data);
				memcpy(data, &ubo, sizeof(UniformBufferObject));
				vkUnmapMemory(device, thisObject->materials[k]->uniformBufferMemory);

			}

		}



		{
			UniformBufferObject offScreenUbo = {};

			offScreenUbo.modelMat = glm::mat4(1.0);

			offScreenUbo.viewMat = ubo.viewMat;
			offScreenUbo.projMat = ubo.projMat;
			offScreenUbo.viewProjMat = ubo.viewProjMat;
			offScreenUbo.InvViewProjMat = ubo.InvViewProjMat;
			offScreenUbo.modelViewProjMat = offScreenUbo.viewProjMat;
			offScreenUbo.InvTransposeMat = offScreenUbo.modelMat;
			offScreenUbo.cameraWorldPos = ubo.cameraWorldPos;

			offScreenPlane->updateVertexBuffer(offScreenUbo.InvViewProjMat);
			offScreenPlaneforPostProcess->updateVertexBuffer(offScreenUbo.InvViewProjMat);

			void* data;
			vkMapMemory(device, lightingMaterial->uniformBufferMemory, 0, sizeof(UniformBufferObject), 0, &data);
			memcpy(data, &offScreenUbo, sizeof(UniformBufferObject));
			vkUnmapMemory(device, lightingMaterial->uniformBufferMemory);

			void* directionLightsData;

			vkMapMemory(device, voxelConetracingMaterial->directionalLightBufferMemory, 0, sizeof(DirectionalLight) * directionLights.size(), 0, &directionLightsData);
			memcpy(directionLightsData, &directionLights[0], sizeof(DirectionalLight) * directionLights.size());
			vkUnmapMemory(device, voxelConetracingMaterial->directionalLightBufferMemory);

			vkMapMemory(device, lightingMaterial->directionalLightBufferMemory, 0, sizeof(DirectionalLight) * directionLights.size(), 0, &directionLightsData);
			memcpy(directionLightsData, &directionLights[0], sizeof(DirectionalLight) * directionLights.size());
			vkUnmapMemory(device, lightingMaterial->directionalLightBufferMemory);
		}

		if (bDeubDisply)
		{
			{
				UniformBufferObject debugDisplayUbo = {};

				debugDisplayUbo.modelMat = glm::mat4(1.0);
				debugDisplayUbo.viewMat = ubo.viewMat;
				debugDisplayUbo.projMat = ubo.projMat;
				debugDisplayUbo.viewProjMat = ubo.viewProjMat;
				debugDisplayUbo.InvViewProjMat = ubo.InvViewProjMat;
				debugDisplayUbo.modelViewProjMat = debugDisplayUbo.viewProjMat;
				debugDisplayUbo.InvTransposeMat = debugDisplayUbo.modelMat;
				debugDisplayUbo.cameraWorldPos = ubo.cameraWorldPos;

				debugDisplayPlane->updateVertexBuffer(debugDisplayUbo.InvViewProjMat);

				for (size_t i = 0; i < NUM_DEBUGDISPLAY; i++)
				{
					void* data;
					vkMapMemory(device, debugDisplayMaterials[i]->uniformBufferMemory, 0, sizeof(UniformBufferObject), 0, &data);
					memcpy(data, &debugDisplayUbo, sizeof(UniformBufferObject));
					vkUnmapMemory(device, debugDisplayMaterials[i]->uniformBufferMemory);
				}
			}
		}

		for (size_t i = 0; i < postProcessStages.size(); i++)
		{
			UniformBufferObject offScreenUbo = {};

			offScreenUbo.modelMat = glm::mat4(1.0);

			VoxelRenderMaterial* isVXGIMat = dynamic_cast<VoxelRenderMaterial*>(postProcessStages[i]->material);

			if (isVXGIMat != NULL)
			{
				offScreenUbo.modelMat = voxelizator.standardObject->modelMat;
			}

			offScreenUbo.viewMat = ubo.viewMat;
			offScreenUbo.projMat = ubo.projMat;
			offScreenUbo.viewProjMat = ubo.viewProjMat;
			offScreenUbo.InvViewProjMat = ubo.InvViewProjMat;
			offScreenUbo.modelViewProjMat = offScreenUbo.viewProjMat * offScreenUbo.modelMat;
			offScreenUbo.InvTransposeMat = offScreenUbo.modelMat;
			offScreenUbo.cameraWorldPos = ubo.cameraWorldPos;

			offScreenPlaneforPostProcess->updateVertexBuffer(offScreenUbo.InvViewProjMat);

			void* data;
			vkMapMemory(device, postProcessStages[i]->material->uniformBufferMemory, 0, sizeof(UniformBufferObject), 0, &data);
			memcpy(data, &offScreenUbo, sizeof(UniformBufferObject));
			vkUnmapMemory(device, postProcessStages[i]->material->uniformBufferMemory);

			BlurMaterial* isBlurMat = dynamic_cast<BlurMaterial*>(postProcessStages[i]->material);
			ComputeBlurMaterial* isComputeBlurMat = dynamic_cast<ComputeBlurMaterial*>(postProcessStages[i]->material);

			if (isBlurMat != NULL)
			{
				BlurUniformBufferObject blurUbo;

				blurUbo.widthGap = isBlurMat->extent.x * isBlurMat->widthScale;
				blurUbo.heightGap = isBlurMat->extent.y * isBlurMat->heightScale;

				void* data;
				vkMapMemory(device, isBlurMat->blurUniformBufferMemory, 0, sizeof(BlurUniformBufferObject), 0, &data);
				memcpy(data, &blurUbo, sizeof(BlurUniformBufferObject));
				vkUnmapMemory(device, isBlurMat->blurUniformBufferMemory);
			}
			else if (isComputeBlurMat != NULL)
			{
				BlurUniformBufferObject blurUbo;
				blurUbo.widthGap = postProcessStages[i]->getImageSize().x;
				blurUbo.heightGap = postProcessStages[i]->getImageSize().y;
				void* data;
				vkMapMemory(device, isComputeBlurMat->blurUniformBufferMemory, 0, sizeof(BlurUniformBufferObject), 0, &data);
				memcpy(data, &blurUbo, sizeof(BlurUniformBufferObject));
				vkUnmapMemory(device, isComputeBlurMat->blurUniformBufferMemory);
			}

		}

		{
			UniformBufferObject offScreenUbo = {};

			offScreenUbo.modelMat = glm::mat4(1.0);
			offScreenUbo.viewMat = ubo.viewMat;
			offScreenUbo.projMat = ubo.projMat;
			offScreenUbo.viewProjMat = ubo.viewProjMat;
			offScreenUbo.InvViewProjMat = ubo.InvViewProjMat;
			offScreenUbo.modelViewProjMat = offScreenUbo.viewProjMat;
			offScreenUbo.InvTransposeMat = offScreenUbo.modelMat;
			offScreenUbo.cameraWorldPos = ubo.cameraWorldPos;

			offScreenPlaneforPostProcess->updateVertexBuffer(offScreenUbo.InvViewProjMat);

			void* data;
			vkMapMemory(device, frameBufferMaterial->uniformBufferMemory, 0, sizeof(UniformBufferObject), 0, &data);
			memcpy(data, &offScreenUbo, sizeof(UniformBufferObject));
			vkUnmapMemory(device, frameBufferMaterial->uniformBufferMemory);
		}

	}

	// GOOD
	void createImageViews()
	{
		// G-buffers
		gBufferImageViews.resize(gBufferImages.size());
		for (uint32_t i = 0; i < gBufferImages.size(); i++)
		{
			if (i == NORMAL_COLOR)
				gBufferImageViews[i] = createImageView(gBufferImages[i], VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
			else
				gBufferImageViews[i] = createImageView(gBufferImages[i], VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
		}
		// sceneImage
		sceneImageView = createImageView(sceneImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	}
	// GOOD
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &imageView));

		return imageView;
	}

	// GOOD
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
	{
		VkImageCreateInfo imageInfo = vks::initializers::imageCreateInfo();
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK_RESULT(vkCreateImage(device, &imageInfo, nullptr, &image));

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);
		VkMemoryAllocateInfo allocInfo = vks::initializers::memoryAllocateInfo();
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
		VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory));

		vkBindImageMemory(device, image, imageMemory, 0);
	}
	// GOOD
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	// GOOD
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandPool commandPool)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool);

		VkImageMemoryBarrier barrier = vks::initializers::imageMemoryBarrier();
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;

		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {  // has stencil component
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}
		else {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else {
			throw std::invalid_argument("unsupported layout transition!");
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		endSingleTimeCommands(commandPool, commandBuffer, queue /*objectDrawQueue*/);
	}
	// GOOD
	VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool)
	{
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, commandPool, false);

		VkCommandBufferBeginInfo beginInfo = vks::initializers::commandBufferBeginInfo();
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}
	// GOOD
	void endSingleTimeCommands(VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue queue)
	{
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo = vks::initializers::submitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(queue);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}





	/*Assets*/

	// TODO: Change voxelizator.Initialize (Don't directly play with surface)
	void LoadObjects()
	{
		static const auto& modelBaseDir = getAssetPath() + "models/vct/";

		Object* Chromie = new Object;
		Chromie->init(device, physicalDevice, cmdPool /*deferredCommandPool*/, queue /*objectDrawQueue*/, modelBaseDir + "Chromie.obj", 0, false);
		Chromie->scale = glm::vec3(0.1f);
		Chromie->UpdateOrbit(0.0f, 85.0f, 0.0);
		Chromie->position = glm::vec3(3.0, -0.05, -0.25);
		Chromie->update();
		//Chromie->bRoll = true;
		//Chromie->rollSpeed = 10.0f;
		Chromie->connectMaterial(AssetDatabase::LoadMaterial("standard_material2"), 0);
		objectManager.push_back(Chromie);

		Object* Cerberus = new Object;
		Cerberus->init(device, physicalDevice, cmdPool /*deferredCommandPool*/, queue /*objectDrawQueue*/, modelBaseDir + "Cerberus.obj", 0, false);
		Cerberus->scale = glm::vec3(3.0f);
		Cerberus->position = glm::vec3(0.0, 3.0, -0.25);
		Cerberus->update();
		Cerberus->bRoll = true;
		Cerberus->rollSpeed = 17.0f;
		Cerberus->connectMaterial(AssetDatabase::LoadMaterial("standard_material3"), 0);
		objectManager.push_back(Cerberus);

		/*
		Object *Johanna = new Object;
		Johanna->init(device, physicalDevice, deferredCommandPool, objectDrawQueue, "objects/Johanna.obj", 0, false);
		Johanna->scale = glm::vec3(0.3f);
		Johanna->position = glm::vec3(-1.0, -0.05, -0.25);
		Johanna->update();
		Johanna->bRoll = true;
		Johanna->rollSpeed = 6.0f;
		Johanna->connectMaterial(AssetDatabase::LoadMaterial("standard_material"), 0);
		objectManager.push_back(Johanna);
		*/

		Object* sponza = new Object;
		sponza->init(device, physicalDevice, cmdPool /*deferredCommandPool*/, queue /*objectDrawQueue*/, modelBaseDir + "sponza.obj", -1, true);
		sponza->scale = glm::vec3(0.01f);
		sponza->position = glm::vec3(0.0, 0.0, 0.0);
		sponza->update();
		ConnectSponzaMaterials(sponza);
		objectManager.push_back(sponza);


		voxelizator.Initialize(device, physicalDevice, swapChain.surface, 1, uint32_t(floor(log2(VOXEL_SIZE))), glm::vec2(1.0, 1.0));
		voxelizator.createImages(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		voxelizator.createCommandPool();

		voxelizator.standardObject = sponza;
		//voxelizator.standardObject = Johanna;
		voxelizator.setMatrices();

		//voxelizator.createBuffers(20000000);

		glm::vec3 EX = voxelizator.standardObject->AABB.Extents * 2.0f;
		voxelizator.createVoxelInfoBuffer(voxelizator.standardObject->AABB.Center, glm::max(glm::max(EX.x, EX.y), EX.z), VOXEL_SIZE, 0.01f);

		voxelizator.setQueue(queue /*objectDrawQueue*/, queue /*TagQueue*/, queue /*AllocationQueue*/, queue /*MipmapQueue*/);
		voxelizator.initMaterial();

		//voxelRenderMaterial->createVoxelInfoBuffer(voxelizator.thisObject->AABB.Center, glm::max(glm::max(EX.x, EX.y), EX.z), VOXEL_SIZE);
	}

	// GOOD
	void ConnectSponzaMaterials(Object* sponza)
	{
		Material* thorn = AssetDatabase::LoadMaterial("thorn");
		sponza->connectMaterial(thorn, 0); //Leaf
		sponza->connectMaterial(thorn, 1); //Material__57
		sponza->connectMaterial(thorn, 275); //Leaf
		sponza->connectMaterial(thorn, 276); //Leaf
		sponza->connectMaterial(thorn, 277); //Leaf
		sponza->connectMaterial(thorn, 278); //Leaf
		sponza->connectMaterial(thorn, 279); //Leaf
		sponza->connectMaterial(thorn, 280); //Leaf
		sponza->connectMaterial(thorn, 281); //Leaf

		Material* fabric_e = AssetDatabase::LoadMaterial("fabric_green");
		sponza->connectMaterial(fabric_e, 282); //curtain_red
		sponza->connectMaterial(fabric_e, 285); //curtain_red
		sponza->connectMaterial(fabric_e, 287); //curtain_red

		Material* fabric_g = AssetDatabase::LoadMaterial("curtain_blue");
		sponza->connectMaterial(fabric_g, 320); //curtain_green
		sponza->connectMaterial(fabric_g, 326); //curtain_green
		sponza->connectMaterial(fabric_g, 329); //curtain_green

		Material* fabric_c = AssetDatabase::LoadMaterial("curtain_red");
		sponza->connectMaterial(fabric_c, 321); //curtain_red
		sponza->connectMaterial(fabric_c, 323); //curtain_red
		sponza->connectMaterial(fabric_c, 325); //curtain_red
		sponza->connectMaterial(fabric_c, 328); //curtain_red

		Material* fabric_f = AssetDatabase::LoadMaterial("curtain_green");
		sponza->connectMaterial(fabric_f, 322); //curtain_blue
		sponza->connectMaterial(fabric_f, 324); //curtain_blue
		sponza->connectMaterial(fabric_f, 327); //curtain_blue

		Material* fabric_a = AssetDatabase::LoadMaterial("fabric_red");
		sponza->connectMaterial(fabric_a, 283); //fabric_red
		sponza->connectMaterial(fabric_a, 286); //fabric_red
		sponza->connectMaterial(fabric_a, 289); //fabric_red

		Material* fabric_d = AssetDatabase::LoadMaterial("fabric_blue");
		sponza->connectMaterial(fabric_d, 284); //fabric_blue
		sponza->connectMaterial(fabric_d, 288); //fabric_blue

		Material* chain = AssetDatabase::LoadMaterial("chain");
		sponza->connectMaterial(chain, 330); //chain
		sponza->connectMaterial(chain, 331); //chain
		sponza->connectMaterial(chain, 332); //chain
		sponza->connectMaterial(chain, 333); //chain

		sponza->connectMaterial(chain, 339); //chain
		sponza->connectMaterial(chain, 340); //chain
		sponza->connectMaterial(chain, 341); //chain
		sponza->connectMaterial(chain, 342); //chain

		sponza->connectMaterial(chain, 348); //chain
		sponza->connectMaterial(chain, 349); //chain
		sponza->connectMaterial(chain, 350); //chain
		sponza->connectMaterial(chain, 351); //chain

		sponza->connectMaterial(chain, 357); //chain
		sponza->connectMaterial(chain, 358); //chain
		sponza->connectMaterial(chain, 359); //chain
		sponza->connectMaterial(chain, 360); //chain

		Material* vase_hanging = AssetDatabase::LoadMaterial("vase_hanging");
		sponza->connectMaterial(vase_hanging, 334); //vase_hanging
		sponza->connectMaterial(vase_hanging, 335); //vase_hanging
		sponza->connectMaterial(vase_hanging, 336); //vase_hanging
		sponza->connectMaterial(vase_hanging, 337); //vase_hanging
		sponza->connectMaterial(vase_hanging, 338); //vase_hanging
		sponza->connectMaterial(vase_hanging, 343); //vase_hanging
		sponza->connectMaterial(vase_hanging, 344); //vase_hanging
		sponza->connectMaterial(vase_hanging, 345); //vase_hanging
		sponza->connectMaterial(vase_hanging, 346); //vase_hanging
		sponza->connectMaterial(vase_hanging, 347); //vase_hanging
		sponza->connectMaterial(vase_hanging, 352); //vase_hanging
		sponza->connectMaterial(vase_hanging, 353); //vase_hanging
		sponza->connectMaterial(vase_hanging, 354); //vase_hanging
		sponza->connectMaterial(vase_hanging, 355); //vase_hanging
		sponza->connectMaterial(vase_hanging, 356); //vase_hanging
		sponza->connectMaterial(vase_hanging, 361); //vase_hanging
		sponza->connectMaterial(vase_hanging, 362); //vase_hanging
		sponza->connectMaterial(vase_hanging, 363); //vase_hanging
		sponza->connectMaterial(vase_hanging, 364); //vase_hanging
		sponza->connectMaterial(vase_hanging, 365); //vase_hanging

		Material* vase_round = AssetDatabase::LoadMaterial("vase_round");
		sponza->connectMaterial(vase_round, 2); //vase_round
		sponza->connectMaterial(vase_round, 366); //Material__57
		sponza->connectMaterial(vase_round, 367); //Material__57
		sponza->connectMaterial(vase_round, 368); //Material__57
		sponza->connectMaterial(vase_round, 369); //Material__57
		sponza->connectMaterial(vase_round, 370); //Material__57
		sponza->connectMaterial(vase_round, 371); //Material__57
		sponza->connectMaterial(vase_round, 372); //Material__57

		Material* vase = AssetDatabase::LoadMaterial("vase");
		sponza->connectMaterial(vase, 373); //vase
		sponza->connectMaterial(vase, 374); //vase
		sponza->connectMaterial(vase, 375); //vase
		sponza->connectMaterial(vase, 376); //vase

		sponza->connectMaterial(AssetDatabase::LoadMaterial("lion_back"), 3); //Material__298

		Material* lion = AssetDatabase::LoadMaterial("lion");
		sponza->connectMaterial(lion, 377); //Material__25
		sponza->connectMaterial(lion, 378); //Material__25

		sponza->connectMaterial(AssetDatabase::LoadMaterial("fabric_red"), 4); //16___Default

		Material* bricks = AssetDatabase::LoadMaterial("bricks");
		sponza->connectMaterial(bricks, 5); //bricks
		sponza->connectMaterial(bricks, 6); //bricks
		sponza->connectMaterial(bricks, 34); //bricks
		sponza->connectMaterial(bricks, 36); //bricks
		sponza->connectMaterial(bricks, 66); //bricks
		sponza->connectMaterial(bricks, 68); //bricks
		sponza->connectMaterial(bricks, 69); //bricks
		sponza->connectMaterial(bricks, 75); //bricks
		sponza->connectMaterial(bricks, 116); //bricks
		sponza->connectMaterial(bricks, 258); //bricks
		sponza->connectMaterial(bricks, 379); //bricks
		sponza->connectMaterial(bricks, 382); //bricks

		Material* roof = AssetDatabase::LoadMaterial("roof");
		sponza->connectMaterial(roof, 380); //roof
		sponza->connectMaterial(roof, 381); //roof


		Material* arch = AssetDatabase::LoadMaterial("arch");
		sponza->connectMaterial(arch, 7); //arch
		sponza->connectMaterial(arch, 17); //arch
		sponza->connectMaterial(arch, 20); //arch
		sponza->connectMaterial(arch, 21); //arch
		sponza->connectMaterial(arch, 37); //arch
		sponza->connectMaterial(arch, 39); //arch
		sponza->connectMaterial(arch, 41); //arch
		sponza->connectMaterial(arch, 43); //arch
		sponza->connectMaterial(arch, 45); //arch
		sponza->connectMaterial(arch, 47); //arch
		sponza->connectMaterial(arch, 49); //arch
		sponza->connectMaterial(arch, 51); //arch
		sponza->connectMaterial(arch, 53); //arch
		sponza->connectMaterial(arch, 55); //arch
		sponza->connectMaterial(arch, 56); //arch
		sponza->connectMaterial(arch, 57); //arch
		sponza->connectMaterial(arch, 58); //arch
		sponza->connectMaterial(arch, 59); //arch
		sponza->connectMaterial(arch, 60); //arch
		sponza->connectMaterial(arch, 61); //arch
		sponza->connectMaterial(arch, 62); //arch
		sponza->connectMaterial(arch, 63); //arch
		sponza->connectMaterial(arch, 64); //arch
		sponza->connectMaterial(arch, 65); //arch
		sponza->connectMaterial(arch, 67); //arch
		sponza->connectMaterial(arch, 122); //arch
		sponza->connectMaterial(arch, 123); //arch
		sponza->connectMaterial(arch, 124); //arch

		Material* ceiling = AssetDatabase::LoadMaterial("ceiling");
		sponza->connectMaterial(ceiling, 8); //ceiling
		sponza->connectMaterial(ceiling, 19); //ceiling
		sponza->connectMaterial(ceiling, 35); //ceiling
		sponza->connectMaterial(ceiling, 38); //ceiling
		sponza->connectMaterial(ceiling, 40); //ceiling
		sponza->connectMaterial(ceiling, 42); //ceiling
		sponza->connectMaterial(ceiling, 44); //ceiling
		sponza->connectMaterial(ceiling, 46); //ceiling
		sponza->connectMaterial(ceiling, 48); //ceiling
		sponza->connectMaterial(ceiling, 50); //ceiling
		sponza->connectMaterial(ceiling, 52); //ceiling
		sponza->connectMaterial(ceiling, 54); //ceiling

		Material* column_a = AssetDatabase::LoadMaterial("column_a");
		sponza->connectMaterial(column_a, 9); //column_a
		sponza->connectMaterial(column_a, 10); //column_a
		sponza->connectMaterial(column_a, 11); //column_a
		sponza->connectMaterial(column_a, 12); //column_a
		sponza->connectMaterial(column_a, 13); //column_a
		sponza->connectMaterial(column_a, 14); //column_a
		sponza->connectMaterial(column_a, 15); //column_a
		sponza->connectMaterial(column_a, 16); //column_a
		sponza->connectMaterial(column_a, 118); //column_a
		sponza->connectMaterial(column_a, 119); //column_a
		sponza->connectMaterial(column_a, 120); //column_a
		sponza->connectMaterial(column_a, 121); //column_a

		Material* column_b = AssetDatabase::LoadMaterial("column_b");
		sponza->connectMaterial(column_b, 125); //column_b
		sponza->connectMaterial(column_b, 126); //column_b
		sponza->connectMaterial(column_b, 127); //column_b
		sponza->connectMaterial(column_b, 128); //column_b
		sponza->connectMaterial(column_b, 129); //column_b
		sponza->connectMaterial(column_b, 130); //column_b
		sponza->connectMaterial(column_b, 131); //column_b
		sponza->connectMaterial(column_b, 132); //column_b
		sponza->connectMaterial(column_b, 133); //column_b
		sponza->connectMaterial(column_b, 134); //column_b
		sponza->connectMaterial(column_b, 135); //column_b
		sponza->connectMaterial(column_b, 136); //column_b
		sponza->connectMaterial(column_b, 137); //column_b
		sponza->connectMaterial(column_b, 138); //column_b
		sponza->connectMaterial(column_b, 139); //column_b
		sponza->connectMaterial(column_b, 140); //column_b
		sponza->connectMaterial(column_b, 141); //column_b
		sponza->connectMaterial(column_b, 142); //column_b
		sponza->connectMaterial(column_b, 143); //column_b
		sponza->connectMaterial(column_b, 144); //column_b
		sponza->connectMaterial(column_b, 145); //column_b
		sponza->connectMaterial(column_b, 146); //column_b
		sponza->connectMaterial(column_b, 147); //column_b
		sponza->connectMaterial(column_b, 148); //column_b
		sponza->connectMaterial(column_b, 149); //column_b
		sponza->connectMaterial(column_b, 150); //column_b
		sponza->connectMaterial(column_b, 151); //column_b
		sponza->connectMaterial(column_b, 152); //column_b
		sponza->connectMaterial(column_b, 153); //column_b
		sponza->connectMaterial(column_b, 154); //column_b
		sponza->connectMaterial(column_b, 155); //column_b
		sponza->connectMaterial(column_b, 156); //column_b
		sponza->connectMaterial(column_b, 157); //column_b
		sponza->connectMaterial(column_b, 158); //column_b
		sponza->connectMaterial(column_b, 159); //column_b
		sponza->connectMaterial(column_b, 160); //column_b
		sponza->connectMaterial(column_b, 161); //column_b
		sponza->connectMaterial(column_b, 162); //column_b
		sponza->connectMaterial(column_b, 163); //column_b
		sponza->connectMaterial(column_b, 164); //column_b
		sponza->connectMaterial(column_b, 165); //column_b
		sponza->connectMaterial(column_b, 166); //column_b
		sponza->connectMaterial(column_b, 167); //column_b
		sponza->connectMaterial(column_b, 168); //column_b
		sponza->connectMaterial(column_b, 169); //column_b
		sponza->connectMaterial(column_b, 170); //column_b
		sponza->connectMaterial(column_b, 171); //column_b
		sponza->connectMaterial(column_b, 172); //column_b
		sponza->connectMaterial(column_b, 173); //column_b
		sponza->connectMaterial(column_b, 174); //column_b
		sponza->connectMaterial(column_b, 175); //column_b
		sponza->connectMaterial(column_b, 176); //column_b
		sponza->connectMaterial(column_b, 177); //column_b
		sponza->connectMaterial(column_b, 178); //column_b
		sponza->connectMaterial(column_b, 179); //column_b
		sponza->connectMaterial(column_b, 180); //column_b
		sponza->connectMaterial(column_b, 181); //column_b
		sponza->connectMaterial(column_b, 182); //column_b
		sponza->connectMaterial(column_b, 183); //column_b
		sponza->connectMaterial(column_b, 184); //column_b
		sponza->connectMaterial(column_b, 185); //column_b
		sponza->connectMaterial(column_b, 186); //column_b
		sponza->connectMaterial(column_b, 187); //column_b
		sponza->connectMaterial(column_b, 188); //column_b
		sponza->connectMaterial(column_b, 189); //column_b
		sponza->connectMaterial(column_b, 190); //column_b
		sponza->connectMaterial(column_b, 191); //column_b
		sponza->connectMaterial(column_b, 192); //column_b
		sponza->connectMaterial(column_b, 193); //column_b
		sponza->connectMaterial(column_b, 194); //column_b
		sponza->connectMaterial(column_b, 195); //column_b
		sponza->connectMaterial(column_b, 196); //column_b
		sponza->connectMaterial(column_b, 197); //column_b
		sponza->connectMaterial(column_b, 198); //column_b
		sponza->connectMaterial(column_b, 199); //column_b
		sponza->connectMaterial(column_b, 200); //column_b
		sponza->connectMaterial(column_b, 201); //column_b
		sponza->connectMaterial(column_b, 202); //column_b
		sponza->connectMaterial(column_b, 203); //column_b
		sponza->connectMaterial(column_b, 204); //column_b
		sponza->connectMaterial(column_b, 205); //column_b
		sponza->connectMaterial(column_b, 206); //column_b
		sponza->connectMaterial(column_b, 207); //column_b
		sponza->connectMaterial(column_b, 208); //column_b
		sponza->connectMaterial(column_b, 209); //column_b
		sponza->connectMaterial(column_b, 210); //column_b
		sponza->connectMaterial(column_b, 211); //column_b
		sponza->connectMaterial(column_b, 212); //column_b
		sponza->connectMaterial(column_b, 213); //column_b
		sponza->connectMaterial(column_b, 214); //column_b
		sponza->connectMaterial(column_b, 215); //column_b
		sponza->connectMaterial(column_b, 216); //column_b
		sponza->connectMaterial(column_b, 217); //column_b
		sponza->connectMaterial(column_b, 218); //column_b
		sponza->connectMaterial(column_b, 219); //column_b
		sponza->connectMaterial(column_b, 220); //column_b
		sponza->connectMaterial(column_b, 221); //column_b
		sponza->connectMaterial(column_b, 222); //column_b
		sponza->connectMaterial(column_b, 223); //column_b
		sponza->connectMaterial(column_b, 224); //column_b
		sponza->connectMaterial(column_b, 225); //column_b
		sponza->connectMaterial(column_b, 226); //column_b
		sponza->connectMaterial(column_b, 227); //column_b
		sponza->connectMaterial(column_b, 228); //column_b
		sponza->connectMaterial(column_b, 229); //column_b
		sponza->connectMaterial(column_b, 230); //column_b
		sponza->connectMaterial(column_b, 231); //column_b
		sponza->connectMaterial(column_b, 232); //column_b
		sponza->connectMaterial(column_b, 233); //column_b
		sponza->connectMaterial(column_b, 234); //column_b
		sponza->connectMaterial(column_b, 235); //column_b
		sponza->connectMaterial(column_b, 236); //column_b
		sponza->connectMaterial(column_b, 237); //column_b
		sponza->connectMaterial(column_b, 238); //column_b
		sponza->connectMaterial(column_b, 239); //column_b
		sponza->connectMaterial(column_b, 240); //column_b
		sponza->connectMaterial(column_b, 241); //column_b
		sponza->connectMaterial(column_b, 242); //column_b
		sponza->connectMaterial(column_b, 243); //column_b
		sponza->connectMaterial(column_b, 244); //column_b
		sponza->connectMaterial(column_b, 245); //column_b
		sponza->connectMaterial(column_b, 246); //column_b
		sponza->connectMaterial(column_b, 247); //column_b
		sponza->connectMaterial(column_b, 248); //column_b
		sponza->connectMaterial(column_b, 249); //column_b
		sponza->connectMaterial(column_b, 250); //column_b
		sponza->connectMaterial(column_b, 251); //column_b
		sponza->connectMaterial(column_b, 252); //column_b
		sponza->connectMaterial(column_b, 253); //column_b
		sponza->connectMaterial(column_b, 254); //column_b
		sponza->connectMaterial(column_b, 255); //column_b
		sponza->connectMaterial(column_b, 256); //column_b
		sponza->connectMaterial(column_b, 257); //column_b

		Material* column_c = AssetDatabase::LoadMaterial("column_c");
		sponza->connectMaterial(column_c, 22); //column_c
		sponza->connectMaterial(column_c, 23); //column_c
		sponza->connectMaterial(column_c, 24); //column_c
		sponza->connectMaterial(column_c, 25); //column_c
		sponza->connectMaterial(column_c, 26); //column_c
		sponza->connectMaterial(column_c, 27); //column_c
		sponza->connectMaterial(column_c, 28); //column_c
		sponza->connectMaterial(column_c, 29); //column_c
		sponza->connectMaterial(column_c, 30); //column_c
		sponza->connectMaterial(column_c, 31); //column_c
		sponza->connectMaterial(column_c, 32); //column_c
		sponza->connectMaterial(column_c, 33); //column_c
		sponza->connectMaterial(column_c, 76); //column_c
		sponza->connectMaterial(column_c, 77); //column_c
		sponza->connectMaterial(column_c, 78); //column_c
		sponza->connectMaterial(column_c, 79); //column_c
		sponza->connectMaterial(column_c, 80); //column_c
		sponza->connectMaterial(column_c, 81); //column_c
		sponza->connectMaterial(column_c, 82); //column_c
		sponza->connectMaterial(column_c, 83); //column_c
		sponza->connectMaterial(column_c, 84); //column_c
		sponza->connectMaterial(column_c, 85); //column_c
		sponza->connectMaterial(column_c, 86); //column_c
		sponza->connectMaterial(column_c, 87); //column_c
		sponza->connectMaterial(column_c, 88); //column_c
		sponza->connectMaterial(column_c, 89); //column_c
		sponza->connectMaterial(column_c, 90); //column_c
		sponza->connectMaterial(column_c, 91); //column_c
		sponza->connectMaterial(column_c, 92); //column_c
		sponza->connectMaterial(column_c, 93); //column_c
		sponza->connectMaterial(column_c, 94); //column_c
		sponza->connectMaterial(column_c, 95); //column_c
		sponza->connectMaterial(column_c, 96); //column_c
		sponza->connectMaterial(column_c, 97); //column_c
		sponza->connectMaterial(column_c, 98); //column_c
		sponza->connectMaterial(column_c, 99); //column_c
		sponza->connectMaterial(column_c, 100); //column_c
		sponza->connectMaterial(column_c, 101); //column_c
		sponza->connectMaterial(column_c, 102); //column_c
		sponza->connectMaterial(column_c, 103); //column_c
		sponza->connectMaterial(column_c, 104); //column_c
		sponza->connectMaterial(column_c, 105); //column_c
		sponza->connectMaterial(column_c, 106); //column_c
		sponza->connectMaterial(column_c, 107); //column_c
		sponza->connectMaterial(column_c, 108); //column_c
		sponza->connectMaterial(column_c, 109); //column_c
		sponza->connectMaterial(column_c, 110); //column_c
		sponza->connectMaterial(column_c, 111); //column_c
		sponza->connectMaterial(column_c, 112); //column_c
		sponza->connectMaterial(column_c, 113); //column_c
		sponza->connectMaterial(column_c, 114); //column_c
		sponza->connectMaterial(column_c, 115); //column_c

		Material* floor = AssetDatabase::LoadMaterial("floor");
		sponza->connectMaterial(floor, 18); //floor
		sponza->connectMaterial(floor, 117); //floor

		Material* detail = AssetDatabase::LoadMaterial("detail");
		sponza->connectMaterial(detail, 70); //detail
		sponza->connectMaterial(detail, 71); //detail
		sponza->connectMaterial(detail, 72); //detail
		sponza->connectMaterial(detail, 73); //detail
		sponza->connectMaterial(detail, 74); //detail

		Material* flagpole = AssetDatabase::LoadMaterial("flagpole");
		sponza->connectMaterial(flagpole, 259); //flagpole
		sponza->connectMaterial(flagpole, 260); //flagpole
		sponza->connectMaterial(flagpole, 261); //flagpole
		sponza->connectMaterial(flagpole, 262); //flagpole
		sponza->connectMaterial(flagpole, 263); //flagpole
		sponza->connectMaterial(flagpole, 264); //flagpole
		sponza->connectMaterial(flagpole, 265); //flagpole
		sponza->connectMaterial(flagpole, 266); //flagpole
		sponza->connectMaterial(flagpole, 267); //flagpole
		sponza->connectMaterial(flagpole, 268); //flagpole
		sponza->connectMaterial(flagpole, 269); //flagpole
		sponza->connectMaterial(flagpole, 270); //flagpole
		sponza->connectMaterial(flagpole, 271); //flagpole
		sponza->connectMaterial(flagpole, 272); //flagpole
		sponza->connectMaterial(flagpole, 273); //flagpole
		sponza->connectMaterial(flagpole, 274); //flagpole
		sponza->connectMaterial(flagpole, 290); //flagpole
		sponza->connectMaterial(flagpole, 291); //flagpole
		sponza->connectMaterial(flagpole, 292); //flagpole
		sponza->connectMaterial(flagpole, 293); //flagpole
		sponza->connectMaterial(flagpole, 294); //flagpole
		sponza->connectMaterial(flagpole, 295); //flagpole
		sponza->connectMaterial(flagpole, 296); //flagpole
		sponza->connectMaterial(flagpole, 297); //flagpole
		sponza->connectMaterial(flagpole, 298); //flagpole
		sponza->connectMaterial(flagpole, 299); //flagpole
		sponza->connectMaterial(flagpole, 300); //flagpole
		sponza->connectMaterial(flagpole, 301); //flagpole
		sponza->connectMaterial(flagpole, 302); //flagpole
		sponza->connectMaterial(flagpole, 303); //flagpole
		sponza->connectMaterial(flagpole, 304); //flagpole
		sponza->connectMaterial(flagpole, 305); //flagpole
		sponza->connectMaterial(flagpole, 306); //flagpole
		sponza->connectMaterial(flagpole, 307); //flagpole
		sponza->connectMaterial(flagpole, 308); //flagpole
		sponza->connectMaterial(flagpole, 309); //flagpole
		sponza->connectMaterial(flagpole, 310); //flagpole
		sponza->connectMaterial(flagpole, 311); //flagpole
		sponza->connectMaterial(flagpole, 312); //flagpole
		sponza->connectMaterial(flagpole, 313); //flagpole
		sponza->connectMaterial(flagpole, 314); //flagpole
		sponza->connectMaterial(flagpole, 315); //flagpole
		sponza->connectMaterial(flagpole, 316); //flagpole
		sponza->connectMaterial(flagpole, 317); //flagpole
		sponza->connectMaterial(flagpole, 318); //flagpole
		sponza->connectMaterial(flagpole, 319); //flagpole
	}
	// GOOD
	void LoadGlobalMaterials(PostProcess* vxgiPostProcess)
	{
		static const auto& shaderBaseDir = getShaderBasePath() + "voxelconetracing/";

		voxelConetracingMaterial = new VoxelConetracingMaterial;
		voxelConetracingMaterial->setDirectionalLights(&directionLights);
		voxelConetracingMaterial->LoadFromFilename(device, physicalDevice, vxgiPostProcess->commandPool, queue /*postProcessQueue*/, "VXGI_material");
		voxelConetracingMaterial->creatDirectionalLightBuffer();
		voxelConetracingMaterial->setShaderPaths(shaderBaseDir + "voxelConeTracing.vert.spv",
			shaderBaseDir + "voxelConeTracing.frag.spv", "", "", "", "");
		voxelConetracingMaterial->setScreenScale(vxgiPostProcess->getScreenScale());
		vxgiPostProcess->material = voxelConetracingMaterial;

		lightingMaterial = new LightingMaterial;
		lightingMaterial->setDirectionalLights(&directionLights);
		lightingMaterial->LoadFromFilename(device, physicalDevice, sceneStage->commandPool, queue /*lightingQueue*/, "lighting_material");
		lightingMaterial->creatDirectionalLightBuffer();
		lightingMaterial->setShaderPaths(shaderBaseDir + "lighting.vert.spv",
			shaderBaseDir + "lighting.frag.spv", "", "", "", "");
		sceneStage->material = lightingMaterial;

		debugDisplayMaterials.resize(NUM_DEBUGDISPLAY);
		for (size_t i = 0; i < NUM_DEBUGDISPLAY; i++)
		{
			debugDisplayMaterials[i] = new DebugDisplayMaterial;
			debugDisplayMaterials[i]->LoadFromFilename(device, physicalDevice, cmdPool /*deferredCommandPool*/, queue /*lightingQueue*/, "debugDisplay_material");
			debugDisplayMaterials[i]->setShaderPaths(shaderBaseDir + "debug.vert.spv",
				shaderBaseDir + "debug" + std::to_string(i) + ".frag.spv", "", "", "", "");
		}
	}
	// GOOD
	// Initialize materialManager
	void LoadObjectMaterials()
	{
		static const auto& textureBaseDir = getAssetPath() + "textures/vct/";

		LoadObjectMaterial("standard_material", textureBaseDir + "storm_hero_d3crusaderf_base_diff.tga", textureBaseDir + "storm_hero_d3crusaderf_base_spec.tga", textureBaseDir + "storm_hero_d3crusaderf_base_norm.tga", textureBaseDir + "storm_hero_d3crusaderf_base_emis.tga");
		LoadObjectMaterial("standard_material2", textureBaseDir + "storm_hero_chromie_ultimate_diff.tga", textureBaseDir + "storm_hero_chromie_ultimate_spec.tga", textureBaseDir + "storm_hero_chromie_ultimate_norm.tga", textureBaseDir + "storm_hero_chromie_ultimate_emis.tga");
		LoadObjectMaterial("standard_material3", textureBaseDir + "Cerberus/Cerberus_A.tga", textureBaseDir + "Cerberus/Cerberus_S.tga", textureBaseDir + "Cerberus/Cerberus_N.tga", textureBaseDir + "Cerberus/Cerberus_E.tga");

		//arch
		LoadObjectMaterial("arch", textureBaseDir + "sponza/arch/arch_albedo.tga", textureBaseDir + "sponza/arch/arch_spec.tga", textureBaseDir + "sponza/arch/arch_norm.tga", textureBaseDir + "sponza/no_emis.tga");

		//bricks
		LoadObjectMaterial("bricks", textureBaseDir + "sponza/bricks/bricks_albedo.tga", textureBaseDir + "sponza/bricks/bricks_spec.tga", textureBaseDir + "sponza/bricks/bricks_norm.tga", textureBaseDir + "sponza/no_emis.tga");

		//ceiling
		LoadObjectMaterial("ceiling", textureBaseDir + "sponza/ceiling/ceiling_albedo.tga", textureBaseDir + "sponza/ceiling/ceiling_spec.tga", textureBaseDir + "sponza/ceiling/ceiling_norm.tga", textureBaseDir + "sponza/no_emis.tga");

		//chain
		LoadObjectMaterial("chain", textureBaseDir + "sponza/chain/chain_albedo.tga", textureBaseDir + "sponza/chain/chain_spec.tga", textureBaseDir + "sponza/chain/chain_norm.tga", textureBaseDir + "sponza/no_emis.tga");

		//column_a
		LoadObjectMaterial("column_a", textureBaseDir + "sponza/column/column_a_albedo.tga", textureBaseDir + "sponza/column/column_a_spec.tga", textureBaseDir + "sponza/column/column_a_norm.tga", textureBaseDir + "sponza/no_emis.tga");
		//column_b
		LoadObjectMaterial("column_b", textureBaseDir + "sponza/column/column_b_albedo.tga", textureBaseDir + "sponza/column/column_b_spec.tga", textureBaseDir + "sponza/column/column_b_norm.tga", textureBaseDir + "sponza/no_emis.tga");
		//column_c
		LoadObjectMaterial("column_c", textureBaseDir + "sponza/column/column_c_albedo.tga", textureBaseDir + "sponza/column/column_c_spec.tga", textureBaseDir + "sponza/column/column_c_norm.tga", textureBaseDir + "sponza/no_emis.tga");

		//curtain_blue
		LoadObjectMaterial("curtain_blue", textureBaseDir + "sponza/curtain/sponza_curtain_blue_albedo.tga", textureBaseDir + "sponza/curtain/sponza_curtain_blue_spec.tga", textureBaseDir + "sponza/curtain/sponza_curtain_norm.tga", textureBaseDir + "sponza/no_emis.tga");
		//curtain_green
		LoadObjectMaterial("curtain_green", textureBaseDir + "sponza/curtain/sponza_curtain_green_albedo.tga", textureBaseDir + "sponza/curtain/sponza_curtain_green_spec.tga", textureBaseDir + "sponza/curtain/sponza_curtain_norm.tga", textureBaseDir + "sponza/no_emis.tga");
		//curtain_red
		LoadObjectMaterial("curtain_red", textureBaseDir + "sponza/curtain/sponza_curtain_red_albedo.tga", textureBaseDir + "sponza/curtain/sponza_curtain_red_spec.tga", textureBaseDir + "sponza/curtain/sponza_curtain_norm.tga", textureBaseDir + "sponza/no_emis.tga");
		
		//detail
		LoadObjectMaterial("detail", textureBaseDir + "sponza/detail/detail_albedo.tga", textureBaseDir + "sponza/detail/detail_spec.tga", textureBaseDir + "sponza/detail/detail_norm.tga", textureBaseDir + "sponza/no_emis.tga");
		
		//fabric_blue
		LoadObjectMaterial("fabric_blue", textureBaseDir + "sponza/fabric/fabric_blue_albedo.tga", textureBaseDir + "sponza/fabric/fabric_blue_spec.tga", textureBaseDir + "sponza/fabric/fabric_norm.tga", textureBaseDir + "sponza/no_emis.tga");
		//fabric_green
		LoadObjectMaterial("fabric_green", textureBaseDir + "sponza/fabric/fabric_green_albedo.tga", textureBaseDir + "sponza/fabric/fabric_green_spec.tga", textureBaseDir + "sponza/fabric/fabric_norm.tga", textureBaseDir + "sponza/no_emis.tga");
		//fabric_red
		LoadObjectMaterial("fabric_red", textureBaseDir + "sponza/fabric/fabric_red_albedo.tga", textureBaseDir + "sponza/fabric/fabric_red_spec.tga", textureBaseDir + "sponza/fabric/fabric_norm.tga", textureBaseDir + "sponza/no_emis.tga");

		//flagpole
		LoadObjectMaterial("flagpole", textureBaseDir + "sponza/flagpole/flagpole_albedo.tga", textureBaseDir + "sponza/flagpole/flagpole_spec.tga", textureBaseDir + "sponza/flagpole/flagpole_norm.tga", textureBaseDir + "sponza/no_emis.tga");

		//floor
		LoadObjectMaterial("floor", textureBaseDir + "sponza/floor/floor_albedo.tga", textureBaseDir + "sponza/floor/floor_spec.tga", textureBaseDir + "sponza/floor/floor_norm.tga", textureBaseDir + "sponza/no_emis.tga");

		//lion
		LoadObjectMaterial("lion", textureBaseDir + "sponza/lion/lion_albedo.tga", textureBaseDir + "sponza/lion/lion_spec.tga", textureBaseDir + "sponza/lion/lion_norm.tga", textureBaseDir + "sponza/no_emis.tga");
		//lion_back
		LoadObjectMaterial("lion_back", textureBaseDir + "sponza/lion_background/lion_background_albedo.tga", textureBaseDir + "sponza/lion_background/lion_background_spec.tga", textureBaseDir + "sponza/lion_background/lion_background_norm.tga", textureBaseDir + "sponza/no_emis.tga");

		//plant
		LoadObjectMaterial("plant", textureBaseDir + "sponza/plant/vase_plant_albedo.tga", textureBaseDir + "sponza/plant/vase_plant_spec.tga", textureBaseDir + "sponza/plant/vase_plant_norm.tga", textureBaseDir + "sponza/plant/vase_plant_emiss.tga");

		//roof
		LoadObjectMaterial("roof", textureBaseDir + "sponza/roof/roof_albedo.tga", textureBaseDir + "sponza/roof/roof_spec.tga", textureBaseDir + "sponza/roof/roof_norm.tga", textureBaseDir + "sponza/no_emis.tga");

		//thorn
		LoadObjectMaterial("thorn", textureBaseDir + "sponza/thorn/sponza_thorn_albedo.tga", textureBaseDir + "sponza/thorn/sponza_thorn_spec.tga", textureBaseDir + "sponza/thorn/sponza_thorn_norm.tga", textureBaseDir + "sponza/thorn/sponza_thorn_emis.tga");

		//vase
		LoadObjectMaterial("vase", textureBaseDir + "sponza/vase/vase_albedo.tga", textureBaseDir + "sponza/vase/vase_spec.tga", textureBaseDir + "sponza/vase/vase_norm.tga", textureBaseDir + "sponza/no_emis.tga");
		//vase_hanging
		LoadObjectMaterial("vase_hanging", textureBaseDir + "sponza/vase_hanging/vase_hanging_albedo.tga", textureBaseDir + "sponza/vase_hanging/vase_round_spec.tga", textureBaseDir + "sponza/vase_hanging/vase_round_norm.tga", textureBaseDir + "sponza/no_emis.tga");
		//vase_round
		LoadObjectMaterial("vase_round", textureBaseDir + "sponza/vase_hanging/vase_round_albedo.tga", textureBaseDir + "sponza/vase_hanging/vase_round_spec.tga", textureBaseDir + "sponza/vase_hanging/vase_round_norm.tga", textureBaseDir + "sponza/no_emis.tga");
	}
	// GOOD
	void LoadObjectMaterial(std::string name, std::string albedo, std::string specular, std::string normal, std::string emissive)
	{
		static const auto& shaderBaseDir = getShaderBasePath() + "voxelconetracing/";

		ObjectDrawMaterial* tempMat = new ObjectDrawMaterial;
		tempMat->LoadFromFilename(device, physicalDevice, cmdPool /*deferredCommandPool*/, queue /*objectDrawQueue*/, name);
		tempMat->addTexture(AssetDatabase::GetInstance()->LoadAsset<Texture>(albedo));
		tempMat->addTexture(AssetDatabase::GetInstance()->LoadAsset<Texture>(specular));
		tempMat->addTexture(AssetDatabase::GetInstance()->LoadAsset<Texture>(normal));
		tempMat->addTexture(AssetDatabase::GetInstance()->LoadAsset<Texture>(emissive));
		tempMat->setShaderPaths(shaderBaseDir + "shader.vert.spv", shaderBaseDir + "shader.frag.spv", "", "", "", "");
		tempMat->createDescriptorSet();

		materialManager.push_back(tempMat);
	}

	// GOOD
	void LoadTextures()
	{
		static const auto& textureBaseDir = getAssetPath() + "textures/vct/";

		LoadTexture(textureBaseDir + "storm_hero_d3crusaderf_base_diff.tga");
		LoadTexture(textureBaseDir + "storm_hero_d3crusaderf_base_spec.tga");
		LoadTexture(textureBaseDir + "storm_hero_d3crusaderf_base_norm.tga");
		LoadTexture(textureBaseDir + "storm_hero_d3crusaderf_base_emis.tga");

		LoadTexture(textureBaseDir + "storm_hero_chromie_ultimate_diff.tga");
		LoadTexture(textureBaseDir + "storm_hero_chromie_ultimate_spec.tga");
		LoadTexture(textureBaseDir + "storm_hero_chromie_ultimate_norm.tga");
		LoadTexture(textureBaseDir + "storm_hero_chromie_ultimate_emis.tga");

		LoadTexture(textureBaseDir + "Cerberus/Cerberus_A.tga");
		LoadTexture(textureBaseDir + "Cerberus/Cerberus_S.tga");
		LoadTexture(textureBaseDir + "Cerberus/Cerberus_N.tga");
		LoadTexture(textureBaseDir + "Cerberus/Cerberus_E.tga");

		LoadTexture(textureBaseDir + "sponza/no_emis.tga");

		//arch
		LoadTexture(textureBaseDir + "sponza/arch/arch_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/arch/arch_spec.tga");
		LoadTexture(textureBaseDir + "sponza/arch/arch_norm.tga");

		//bricks
		LoadTexture(textureBaseDir + "sponza/bricks/bricks_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/bricks/bricks_spec.tga");
		LoadTexture(textureBaseDir + "sponza/bricks/bricks_norm.tga");

		//celing
		LoadTexture(textureBaseDir + "sponza/ceiling/ceiling_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/ceiling/ceiling_spec.tga");
		LoadTexture(textureBaseDir + "sponza/ceiling/ceiling_norm.tga");

		//column
		LoadTexture(textureBaseDir + "sponza/column/column_a_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/column/column_a_spec.tga");
		LoadTexture(textureBaseDir + "sponza/column/column_a_norm.tga");
		LoadTexture(textureBaseDir + "sponza/column/column_b_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/column/column_b_spec.tga");
		LoadTexture(textureBaseDir + "sponza/column/column_b_norm.tga");
		LoadTexture(textureBaseDir + "sponza/column/column_c_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/column/column_c_spec.tga");
		LoadTexture(textureBaseDir + "sponza/column/column_c_norm.tga");

		//curtain
		LoadTexture(textureBaseDir + "sponza/curtain/sponza_curtain_blue_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/curtain/sponza_curtain_green_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/curtain/sponza_curtain_red_albedo.tga");

		LoadTexture(textureBaseDir + "sponza/curtain/sponza_curtain_blue_spec.tga");
		LoadTexture(textureBaseDir + "sponza/curtain/sponza_curtain_green_spec.tga");
		LoadTexture(textureBaseDir + "sponza/curtain/sponza_curtain_red_spec.tga");

		LoadTexture(textureBaseDir + "sponza/curtain/sponza_curtain_norm.tga");

		//detail
		LoadTexture(textureBaseDir + "sponza/detail/detail_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/detail/detail_spec.tga");
		LoadTexture(textureBaseDir + "sponza/detail/detail_norm.tga");

		//fabric
		LoadTexture(textureBaseDir + "sponza/fabric/fabric_blue_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/fabric/fabric_blue_spec.tga");
		LoadTexture(textureBaseDir + "sponza/fabric/fabric_green_albedo.tga");

		LoadTexture(textureBaseDir + "sponza/fabric/fabric_green_spec.tga");
		LoadTexture(textureBaseDir + "sponza/fabric/fabric_red_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/fabric/fabric_red_spec.tga");

		LoadTexture(textureBaseDir + "sponza/fabric/fabric_norm.tga");

		//flagpole
		LoadTexture(textureBaseDir + "sponza/flagpole/flagpole_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/flagpole/flagpole_spec.tga");
		LoadTexture(textureBaseDir + "sponza/flagpole/flagpole_norm.tga");

		//floor
		LoadTexture(textureBaseDir + "sponza/floor/floor_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/floor/floor_spec.tga");
		LoadTexture(textureBaseDir + "sponza/floor/floor_norm.tga");

		//lion
		LoadTexture(textureBaseDir + "sponza/lion/lion_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/lion/lion_norm.tga");
		LoadTexture(textureBaseDir + "sponza/lion/lion_spec.tga");

		//lion_back
		LoadTexture(textureBaseDir + "sponza/lion_background/lion_background_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/lion_background/lion_background_spec.tga");
		LoadTexture(textureBaseDir + "sponza/lion_background/lion_background_norm.tga");

		//plant
		LoadTexture(textureBaseDir + "sponza/plant/vase_plant_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/plant/vase_plant_spec.tga");
		LoadTexture(textureBaseDir + "sponza/plant/vase_plant_norm.tga");
		LoadTexture(textureBaseDir + "sponza/plant/vase_plant_emiss.tga");

		//roof
		LoadTexture(textureBaseDir + "sponza/roof/roof_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/roof/roof_spec.tga");
		LoadTexture(textureBaseDir + "sponza/roof/roof_norm.tga");

		//thorn
		LoadTexture(textureBaseDir + "sponza/thorn/sponza_thorn_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/thorn/sponza_thorn_spec.tga");
		LoadTexture(textureBaseDir + "sponza/thorn/sponza_thorn_norm.tga");
		LoadTexture(textureBaseDir + "sponza/thorn/sponza_thorn_emis.tga");

		//vase
		LoadTexture(textureBaseDir + "sponza/vase/vase_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/vase/vase_spec.tga");
		LoadTexture(textureBaseDir + "sponza/vase/vase_norm.tga");

		//vase others
		LoadTexture(textureBaseDir + "sponza/vase_hanging/vase_hanging_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/vase_hanging/vase_round_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/vase_hanging/vase_round_spec.tga");
		LoadTexture(textureBaseDir + "sponza/vase_hanging/vase_round_norm.tga");

		//chain
		LoadTexture(textureBaseDir + "sponza/chain/chain_albedo.tga");
		LoadTexture(textureBaseDir + "sponza/chain/chain_spec.tga");
		LoadTexture(textureBaseDir + "sponza/chain/chain_norm.tga");
	}
	// GOOD
	void LoadTexture(std::string path)
	{
		AssetDatabase* instance = AssetDatabase::GetInstance();
		instance->LoadAsset<Texture>(path);
		instance->textureList.push_back(path);
	}










	void draw()  // helper for render(), submitting to cmd queue
	{
		VulkanExampleBase::prepareFrame();  // acquire the next swapchain image

		VkSemaphore prevSemaphore = semaphores.presentComplete;
		VkSemaphore currentSemaphore;

		submitInfo.commandBufferCount = 1;

		//objectDrawQueue
		{
			currentSemaphore = objectDrawCompleteSemaphore;

			submitInfo.pWaitSemaphores = &prevSemaphore;
			submitInfo.pCommandBuffers = &deferredCommandBuffer;
			submitInfo.pSignalSemaphores = &currentSemaphore;
			VK_CHECK_RESULT(vkQueueSubmit(queue /*objectDrawQueue*/, 1, &submitInfo, VK_NULL_HANDLE))
			
			prevSemaphore = currentSemaphore;
		}

		//DrawShadow
		{
			currentSemaphore = standardShadow.semaphore;

			submitInfo.pWaitSemaphores = &prevSemaphore;
			submitInfo.pCommandBuffers = &standardShadow.commandBuffer;
			submitInfo.pSignalSemaphores = &currentSemaphore;
			VK_CHECK_RESULT(vkQueueSubmit(queue /*standardShadow.queue*/, 1, &submitInfo, VK_NULL_HANDLE))

			prevSemaphore = currentSemaphore;
		}

		//postProcessQueue
		{
			//prevSemaphore = voxelizator.createMipmaps(prevSemaphore);
			for (auto& postProcess : postProcessStages)
			{
				currentSemaphore = postProcess->postProcessSemaphore;

				submitInfo.pWaitSemaphores = &prevSemaphore;
				submitInfo.pCommandBuffers = &(postProcess->commandBuffer);
				submitInfo.pSignalSemaphores = &currentSemaphore;
				VK_CHECK_RESULT(vkQueueSubmit(queue /*postProcess->material->queue*/, 1, &submitInfo, VK_NULL_HANDLE))

				prevSemaphore = currentSemaphore;
			}
		}

		//frameQueue
		{
			currentSemaphore = semaphores.renderComplete;

			submitInfo.pWaitSemaphores = &prevSemaphore;
			submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
			submitInfo.pSignalSemaphores = &semaphores.renderComplete;
			VK_CHECK_RESULT(vkQueueSubmit(queue /*presentQueue*/, 1, &submitInfo, VK_NULL_HANDLE))
		}

		VulkanExampleBase::submitFrame();
	}

	/** @brief Prepares all Vulkan resources and functions required to run the sample */
	void prepare() override
	{
		VulkanExampleBase::prepare();

		loadAssets();  // Prepare assets

		deferredSetup();
		shadowSetup();
		initLights();
		prepareUniformBuffers();

		preparePipelines();

		buildCommandBuffers();  // override, build drawCmdBuffers (frame buffers)
		buildDeferredCommandBuffer();
		prepared = true;
	}









	void render()  // replace VulkanExampleBase::renderFrame
	{
		if (!prepared)
			return;
		draw();

		updateUniformBuffers(frameTimer);
		//updateUniformBufferDeferredLights();
		
		//if (camera.updated) 
		//{
		//	updateUniformBufferOffscreen();
		//}
	}

	virtual void viewChanged() override
	{
		//updateUniformBufferOffscreen();
		updateUniformBuffers(frameTimer);
	}









	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
	{
		if (overlay->header("Settings")) {
			if (overlay->comboBox("Display", &debugDisplayTarget, { "Final composition", "Shadows", "Position", "Normals", "Albedo", "Specular" }))
			{
				updateUniformBufferDeferredLights();
			}
			bool shadows = (uboComposition.useShadows == 1);
			if (overlay->checkBox("Shadows", &shadows)) {
				uboComposition.useShadows = shadows;
				updateUniformBufferDeferredLights();
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()
