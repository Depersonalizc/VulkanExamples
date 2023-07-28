/*
* Vulkan Example - Deferred shading with shadows from multiple light sources using geometry shader instancing
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/
#include <unistd.h>
#include "vulkanexamplebase.h"
#include "VulkanFrameBuffer.hpp"

/******************************/
#include "core/PostProcess.h"
#include "core/Shadow.h"
#include "core/Voxelization.h"

#include "assets/Material.h"
#include "assets/Geometry.h"
#include "assets/AssetDatabase.h"

#include "actors/Object.h"
#include "actors/Light.h"
/*****************************/


#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

// Shadowmap properties
#if defined(__ANDROID__)
#define SHADOWMAP_WIDTH 2048
#define SHADOWMAP_HEIGHT 512
#else
#define SHADOWMAP_WIDTH 4096
#define SHADOWMAP_HEIGHT 1024
#endif

//#if defined(__ANDROID__)
//// Use max. screen dimension as deferred framebuffer size
//#define FB_DIM std::max(width,height)
//#else
//#define FB_DIM 2048
//#endif

//// Must match the LIGHT_COUNT define in the shadow and deferred shaders
//#define LIGHT_COUNT 3


class VulkanExample : public VulkanExampleBase
{
public:

	float zNear = 0.1f;
	float zFar = 100.0f;
	int32_t drawMode = 4;
	bool bGbufferView = false;
	bool bRotateMainLight = true;

	float mainLightAngle = 1.0f;
	std::vector<DirectionalLight> directionLights;

	StandardShadow standardShadow;
	Voxelization voxelizer;

	PostProcess* vxgiPP, * sceneStage, * hdrHighlightPP, * horizontalBlurPP, * verticalBlurPP, * horizontalBlurPP2, * verticalBlurPP2;
	PostProcess* theLastPostProcess;

	std::vector<PostProcess*> postProcessStages;

	//VkQueue objectDrawQueue, TagQueue, AllocationQueue, MipmapQueue;
	//VkQueue lightingQueue, postProcessQueue, presentQueue;

	VoxelConetracingMaterial* voxelConetracingMaterial;
	LightingMaterial* lightingMaterial;

	HDRHighlightMaterial* hdrHighlightMaterial;

	BlurMaterial* HBMaterial;
	BlurMaterial* VBMaterial;
	BlurMaterial* HBMaterial2;
	BlurMaterial* VBMaterial2;

	singleTriangular* offScreenPlane;
	//singleTriangular* offScreenPlaneforPostProcess;
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



	VkExtent2D swapChainExtent;







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
		camera.position = { -10.0f, -1.7f, 2.0f };
		camera.setRotation({ 0.0f, 250.0f, 0.0f });
		camera.flipY = true;
		camera.setPerspective(45.0f, (float)width / (float)height, zNear, zFar);
		camera.flipY = false;
		timerSpeed *= 0.25f;

		paused = true;
	}

	~VulkanExample()  // TODO
	{
		AssetDatabase::GetInstance()->cleanUp();
		destroyPlaneGeos();

		standardShadow.cleanUp();
		voxelizer.shutDown();

		destroyGlobalMaterials();
		destroyPostProcessMaterials();
		destroyFrameBufferMaterial();

		for (auto& post : postProcessStages)
		{
			post->cleanUp();
			delete post;
		}

		destroySemaphores();
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
		// Geometry shader support is required for voxelization
		if (deviceFeatures.fragmentStoresAndAtomics) {
			enabledFeatures.fragmentStoresAndAtomics = VK_TRUE;
		}
		else {
			vks::tools::exitFatal("Selected GPU does not support fragment stores!", VK_ERROR_FEATURE_NOT_PRESENT);
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

	// 
	virtual void getDepthFormat() override
	{
		VkFormatProperties formatProps;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, VK_FORMAT_D32_SFLOAT, &formatProps);
		if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			depthFormat = VK_FORMAT_D32_SFLOAT;
		}
		else
		{
			vks::tools::exitFatal("Selected GPU does not support float32 depth format!", VK_ERROR_FORMAT_NOT_SUPPORTED);
		}
	}

	void setupDepthStencil() override
	{
		VkImageCreateInfo imageCI{};
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = depthFormat;
		imageCI.extent = { width, height, 1 };
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;  // IMPORTANT: set VK_IMAGE_USAGE_SAMPLED_BIT!

		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &depthStencil.image));
		VkMemoryRequirements memReqs{};
		vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);

		VkMemoryAllocateInfo memAllloc{};
		memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllloc.allocationSize = memReqs.size;
		memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &depthStencil.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

		VkImageViewCreateInfo imageViewCI{};
		imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCI.image = depthStencil.image;
		imageViewCI.format = depthFormat;
		imageViewCI.subresourceRange.baseMipLevel = 0;
		imageViewCI.subresourceRange.levelCount = 1;
		imageViewCI.subresourceRange.baseArrayLayer = 0;
		imageViewCI.subresourceRange.layerCount = 1;
		imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
		if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
			imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &depthStencil.view));
	}

	void mouseMoved(double x, double y, bool &handled) override
	{
		int32_t dx = (int32_t)mousePos.x - x;
		int32_t dy = (int32_t)mousePos.y - y;

		if (mouseButtons.left) {
			camera.rotate(glm::vec3(-dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
			viewUpdated = true;
		}
		if (mouseButtons.right) {
			camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f));
			viewUpdated = true;
		}
		if (mouseButtons.middle) {
			camera.translate(glm::vec3(-dx * 0.005f, -dy * 0.005f, 0.0f));
			viewUpdated = true;
		}
		
		handled = true;
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
		directionLights.push_back(initLight({ -0.5, 14.618034, -0.4 }, // position
											{ -0.5, 14.5     , -0.4 }, // focus target
											{  1.0, 1.0      ,  1.0 }  // color
											));  // main light
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

	// GOOD
	void loadAssets()
	{
		// Initialize asset database instance
		AssetDatabase::GetInstance();
		AssetDatabase::SetDevice(device, physicalDevice, cmdPool /*deferredCommandPool*/, queue /*objectDrawQueue*/);

		LoadTextures();
		LoadObjectMaterials();
		LoadObjects();    // Load object gemoetry and connect materials. !!ALSO SET voxelizer's buffer and material (TODO: separate this)
		LoadPlaneGeos();
		for (const auto& post : postProcessStages)
		{
			post->offScreenPlane = offScreenPlane;
		}
	}



	// GOOD
	// Build a secondary command buffer for rendering the scene values to the offscreen frame buffer attachments
	void buildDeferredCommandBuffer()
	{
		deferredCommandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		std::array<VkClearValue, NUM_GBUFFERS + 1> clearValues = {};
		clearValues[BASIC_COLOR].color = { 0.0f, 0.0f, 0.0f, 0.0f };
		clearValues[SPECULAR_COLOR].color = { 0.0f, 0.0f, 0.0f, 0.0f };
		clearValues[NORMAL_COLOR].color = { 0.0f, 0.0f, 0.0f, 0.0f };
		clearValues[EMISSIVE_COLOR].color = { 0.0f, 0.0f, 0.0f, 0.0f };
		clearValues[NUM_GBUFFERS].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassInfo = vks::initializers::renderPassBeginInfo();
		renderPassInfo.renderPass = deferredRenderPass;
		renderPassInfo.framebuffer = deferredFrameBuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent.width = width;
		renderPassInfo.renderArea.extent.height = height;
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();


		VK_CHECK_RESULT(vkBeginCommandBuffer(deferredCommandBuffer, &cmdBufInfo));
		vkCmdBeginRenderPass(deferredCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkDeviceSize offsets[] = { 0 };
		for (const auto & object : objectManager)
		{
			for (size_t k = 0; k < object->geos.size(); k++)
			{
				vkCmdBindPipeline(deferredCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object->materials[k]->pipeline);
				vkCmdBindDescriptorSets(deferredCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object->materials[k]->pipelineLayout, 0, 1, &object->materials[k]->descriptorSet, 0, nullptr);

				//VkBuffer vertexBuffers[] = { object->geos[k]->vertexBuffer };
				//VkBuffer indexBuffer = object->geos[k]->indexBuffer;
				vkCmdBindVertexBuffers(deferredCommandBuffer, 0, 1, &(object->geos[k]->vertexBuffer), offsets);
				vkCmdBindIndexBuffer(deferredCommandBuffer, object->geos[k]->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

				vkCmdDrawIndexed(deferredCommandBuffer, static_cast<uint32_t>(object->geos[k]->indices.size()), 1, 0, 0, 0);
			}
		}

		vkCmdEndRenderPass(deferredCommandBuffer);
		VK_CHECK_RESULT(vkEndCommandBuffer(deferredCommandBuffer));
	}


	void buildCommandBuffers() override  // record draw cmd buffers commands
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = { { 0.0f, 0.678431f, 0.902f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassBeginInfo.pClearValues = clearValues.data();

		VkDeviceSize offsets[] = { 0 };
		for (size_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = VulkanExampleBase::frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, frameBufferMaterial->pipeline);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, frameBufferMaterial->pipelineLayout, 0, 1, &frameBufferMaterial->descriptorSet, 0, nullptr);

			//VkBuffer vertexBuffers[] = { offScreenPlane->vertexBuffer };
			//VkBuffer indexBuffer = offScreenPlane->indexBuffer;
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &offScreenPlane->vertexBuffer, offsets);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], offScreenPlane->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdDrawIndexed(drawCmdBuffers[i], static_cast<uint32_t>(offScreenPlane->indices.size()), 1, 0, 0, 0);

			if (bGbufferView)
			{
				for (size_t k = 0; k < NUM_DEBUGDISPLAY; k++)
				{
					vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, debugDisplayMaterials[k]->pipeline);
					vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, debugDisplayMaterials[k]->pipelineLayout, 0, 1, &debugDisplayMaterials[k]->descriptorSet, 0, nullptr);

					//VkBuffer vertexBuffers[] = { debugDisplayPlane->vertexBuffer };
					//VkBuffer indexBuffer = debugDisplayPlane->indexBuffer;
					vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &debugDisplayPlane->vertexBuffer, offsets);
					vkCmdBindIndexBuffer(drawCmdBuffers[i], debugDisplayPlane->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

					vkCmdDrawIndexed(drawCmdBuffers[i], static_cast<uint32_t>(debugDisplayPlane->indices.size()), 1, 0, 0, 0);
				}
			}

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	// GOOD
	void createSemaphores()
	{
		VkSemaphoreCreateInfo semaphoreInfo = vks::initializers::semaphoreCreateInfo();
		VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &objectDrawCompleteSemaphore));
	}
	void destroySemaphores()
	{
		vkDestroySemaphore(device, objectDrawCompleteSemaphore, nullptr);
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
			//VkFormat attDepthFormat;
			//assert(vks::tools::getSupportedDepthFormat(physicalDevice, &attDepthFormat) && "no supported depth format");
			//depthAttachment.format = attDepthFormat;
			depthAttachment.format = depthFormat;
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
		updateDrawMode();

		if (bRotateMainLight)
		{
			mainLightAngle += static_cast<float>(deltaTime) * 0.2f;
			swingMainLight();
			SetDirectionLightMatrices(directionLights[0], 19.0f, 5.0f, 0.0f, 20.0f);

			ShadowUniformBuffer subo = {};
			subo.viewProjMat = directionLights[0].projMat * directionLights[0].viewMat;
			subo.invViewProjMat = glm::inverse(subo.viewProjMat);

			void* shadowdata;
			vkMapMemory(device, lightingMaterial->shadowConstantBufferMemory, 0, sizeof(ShadowUniformBuffer), 0, &shadowdata);
			memcpy(shadowdata, &subo, sizeof(ShadowUniformBuffer));
			vkUnmapMemory(device, lightingMaterial->shadowConstantBufferMemory);
		}

		UniformBufferObject ubo = {};
		ubo.modelMat = glm::mat4(1.0);
		ubo.viewMat = camera.matrices.view;
		ubo.projMat = camera.matrices.perspective;
		ubo.viewProjMat = camera.matrices.viewProj;
		ubo.InvViewProjMat = camera.matrices.invViewProj;
		ubo.modelViewProjMat = ubo.viewProjMat;
		ubo.cameraWorldPos = camera.position;
		ubo.InvTransposeMat = ubo.modelMat;


		for (size_t i = 0; i < objectManager.size(); i++)
		{
			Object* thisObject = objectManager[i];

			if (thisObject->bRoll)
				thisObject->UpdateOrbit(deltaTime * thisObject->rollSpeed, 0.0f, 0.0f);

			ubo.modelMat = thisObject->modelMat;
			ubo.modelViewProjMat = ubo.viewProjMat * thisObject->modelMat;
			//glm::mat4 A = ubo.modelMat;
			//A[3] = glm::vec4(0, 0, 0, 1);
			ubo.InvTransposeMat = glm::transpose(glm::inverse(ubo.modelMat));

			//shadow
			{
				void* data;
				//vkMapMemory(device, thisObject->shadowMaterial->ShadowConstantBufferMemory, 0, sizeof(ShadowUniformBuffer), 0, &data);
				//memcpy(data, &subo, sizeof(ShadowUniformBuffer));
				//vkUnmapMemory(device, thisObject->shadowMaterial->ShadowConstantBufferMemory);

				ObjectUniformBuffer obu = { thisObject->modelMat };
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
			//offScreenPlaneforPostProcess->updateVertexBuffer(offScreenUbo.InvViewProjMat);

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

		if (bGbufferView)
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

		for (size_t i = 0; i < postProcessStages.size(); i++)
		{
			UniformBufferObject offScreenUbo = {};

			offScreenUbo.modelMat = glm::mat4(1.0);

			VoxelRenderMaterial* isVXGIMat = dynamic_cast<VoxelRenderMaterial*>(postProcessStages[i]->material);

			if (isVXGIMat != NULL)
			{
				offScreenUbo.modelMat = voxelizer.standardObject->modelMat;
			}

			offScreenUbo.viewMat = ubo.viewMat;
			offScreenUbo.projMat = ubo.projMat;
			offScreenUbo.viewProjMat = ubo.viewProjMat;
			offScreenUbo.InvViewProjMat = ubo.InvViewProjMat;
			offScreenUbo.modelViewProjMat = offScreenUbo.viewProjMat * offScreenUbo.modelMat;
			offScreenUbo.InvTransposeMat = offScreenUbo.modelMat;
			offScreenUbo.cameraWorldPos = ubo.cameraWorldPos;

			//offScreenPlaneforPostProcess->updateVertexBuffer(offScreenUbo.InvViewProjMat);

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

			//offScreenPlaneforPostProcess->updateVertexBuffer(offScreenUbo.InvViewProjMat);

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

	void LoadPlaneGeos()
	{
		offScreenPlane = new singleTriangular;
		offScreenPlane->LoadFromFilename(device, physicalDevice, cmdPool /*frameBufferCommandPool*/, queue /*lightingQueue*/, "offScreenPlane");

		debugDisplayPlane = new singleQuadral;
		debugDisplayPlane->LoadFromFilename(device, physicalDevice, cmdPool /*frameBufferCommandPool*/, queue /*lightingQueue*/, "debugDisplayPlane");

		//offScreenPlaneforPostProcess = new singleTriangular;
		//offScreenPlaneforPostProcess->LoadFromFilename(device, physicalDevice, sceneStage->commandPool, queue /*postProcessQueue*/, "offScreenPlaneforPostProcess");
	}
	void destroyPlaneGeos()
	{
		delete offScreenPlane;
		delete debugDisplayPlane;
		//delete offScreenPlaneforPostProcess;
	}

	// TODO: Change voxelizer.Initialize (Don't directly access surface)
	void LoadObjects()
	{
		Object* Chromie = new Object;
		Chromie->init(device, physicalDevice, cmdPool /*deferredCommandPool*/, queue /*objectDrawQueue*/, getModelPath("Chromie.obj"), 0, false);
		Chromie->scale = glm::vec3(0.1f);
		Chromie->UpdateOrbit(0.0f, 85.0f, 0.0);
		Chromie->position = glm::vec3(3.0, -0.05, -0.25);
		Chromie->update();
		//Chromie->bRoll = true;
		//Chromie->rollSpeed = 10.0f;
		Chromie->connectMaterial(AssetDatabase::LoadMaterial("standard_material2"), 0);
		objectManager.push_back(Chromie);

		Object* Cerberus = new Object;
		Cerberus->init(device, physicalDevice, cmdPool /*deferredCommandPool*/, queue /*objectDrawQueue*/, getModelPath("Cerberus.obj"), 0, false);
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
		sponza->init(device, physicalDevice, cmdPool /*deferredCommandPool*/, queue /*objectDrawQueue*/, getModelPath("sponza.obj"), -1, true);
		sponza->scale = glm::vec3(0.01f);
		sponza->position = glm::vec3(0.0, 0.0, 0.0);
		sponza->update();
		ConnectSponzaMaterials(sponza);
		objectManager.push_back(sponza);


		{
			voxelizer.standardObject = sponza;
			//voxelizer.standardObject = Johanna;

			voxelizer.setMatrices();
			//voxelizer.createBuffers(20000000);
			glm::vec3 EX = voxelizer.standardObject->AABB.Extents * 2.0f;
			voxelizer.createVoxelInfoBuffer(voxelizer.standardObject->AABB.Center, glm::max(glm::max(EX.x, EX.y), EX.z), VOXEL_SIZE, 0.01f);
			voxelizer.initMaterial();
		}
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

	void LoadFrameBufferMaterial()
	{
		frameBufferMaterial = new FinalRenderingMaterial;
		frameBufferMaterial->LoadFromFilename(device, physicalDevice, cmdPool /*frameBufferCommandPool*/, queue /*presentQueue*/, "frameDisplay_material");
		frameBufferMaterial->setShaderPaths(getShaderPath("postprocess.vert.spv"), getShaderPath("scene.frag.spv"), "", "", "", "");
		//[framebuffer]
		//frameBufferMaterial->setImageViews(standardShadow.outputImageView, depthImageView);
		//frameBufferMaterial->setImageViews(BarrelAndAberrationPostProcess->outputImageView, depthImageView);
		//frameBufferMaterial->setImageViews(voxelizer.outputImageView, depthImageView);
		frameBufferMaterial->setImageViews(theLastPostProcess->outputImageView, depthStencil.view);
		frameBufferMaterial->createDescriptorSet();
		frameBufferMaterial->connectRenderPass(renderPass);
		frameBufferMaterial->createGraphicsPipeline(glm::vec2(width, height), glm::vec2(0.0, 0.0));
	}
	void destroyFrameBufferMaterial()
	{
		delete frameBufferMaterial;
	}

	// GOOD
	void LoadPostProcessMaterials()  // Must be called after all the PP handles have been initialized
	{
		hdrHighlightMaterial->LoadFromFilename(device, physicalDevice, hdrHighlightPP->commandPool, queue /*postProcessQueue*/, "hdrHighlight_material");
		hdrHighlightMaterial->setShaderPaths(getShaderPath("postprocess.vert.spv"), getShaderPath("HDRHighlight.frag.spv"), "", "", "", "");
		hdrHighlightMaterial->setScreenScale(glm::vec2(DOWNSAMPLING_BLOOM, DOWNSAMPLING_BLOOM));
		hdrHighlightMaterial->setImageViews(sceneStage->outputImageView, depthStencil.view);
		hdrHighlightMaterial->createDescriptorSet();
		hdrHighlightMaterial->connectRenderPass(hdrHighlightPP->renderPass);
		hdrHighlightMaterial->createGraphicsPipeline(glm::vec2(hdrHighlightPP->width, hdrHighlightPP->height), glm::vec2(0.0, 0.0));

		HBMaterial->LoadFromFilename(device, physicalDevice, horizontalBlurPP->commandPool, queue /*postProcessQueue*/, "HB_material");
		HBMaterial->setShaderPaths(getShaderPath("postprocess.vert.spv"), getShaderPath("horizontalBlur.frag.spv"), "", "", "", "");
		HBMaterial->setScreenScale(horizontalBlurPP->getScreenScale());
		HBMaterial->setImageViews(hdrHighlightPP->outputImageView, depthStencil.view);
		HBMaterial->createDescriptorSet();
		HBMaterial->connectRenderPass(horizontalBlurPP->renderPass);
		HBMaterial->createGraphicsPipeline(glm::vec2(horizontalBlurPP->width, horizontalBlurPP->height), glm::vec2(0.0, 0.0));

		VBMaterial->LoadFromFilename(device, physicalDevice, verticalBlurPP->commandPool, queue /*postProcessQueue*/, "VB_material");
		VBMaterial->setShaderPaths(getShaderPath("postprocess.vert.spv"), getShaderPath("verticalBlur.frag.spv"), "", "", "", "");
		VBMaterial->setScreenScale(verticalBlurPP->getScreenScale());
		VBMaterial->setImageViews(horizontalBlurPP->outputImageView, depthStencil.view);
		VBMaterial->createDescriptorSet();
		VBMaterial->connectRenderPass(verticalBlurPP->renderPass);
		VBMaterial->createGraphicsPipeline(glm::vec2(verticalBlurPP->width, verticalBlurPP->height), glm::vec2(0.0, 0.0));

		HBMaterial2->LoadFromFilename(device, physicalDevice, horizontalBlurPP2->commandPool, queue /*postProcessQueue*/, "HB2_material");
		HBMaterial2->setShaderPaths(getShaderPath("postprocess.vert.spv"), getShaderPath("horizontalBlur.frag.spv"), "", "", "", "");
		HBMaterial2->setScreenScale(horizontalBlurPP2->getScreenScale());
		HBMaterial2->setImageViews(verticalBlurPP->outputImageView, depthStencil.view);
		HBMaterial2->createDescriptorSet();
		HBMaterial2->connectRenderPass(horizontalBlurPP2->renderPass);
		HBMaterial2->createGraphicsPipeline(glm::vec2(horizontalBlurPP2->width, horizontalBlurPP2->height), glm::vec2(0.0, 0.0));

		VBMaterial2->LoadFromFilename(device, physicalDevice, verticalBlurPP2->commandPool, queue /*postProcessQueue*/, "VB2_material");
		VBMaterial2->setShaderPaths(getShaderPath("postprocess.vert.spv"), getShaderPath("verticalBlur.frag.spv"), "", "", "", "");
		VBMaterial2->setScreenScale(verticalBlurPP2->getScreenScale());
		VBMaterial2->setImageViews(horizontalBlurPP2->outputImageView, depthStencil.view);
		VBMaterial2->createDescriptorSet();
		VBMaterial2->connectRenderPass(verticalBlurPP2->renderPass);
		VBMaterial2->createGraphicsPipeline(glm::vec2(verticalBlurPP2->width, verticalBlurPP2->height), glm::vec2(0.0, 0.0));

		lastPostProcessMaterial->LoadFromFilename(device, physicalDevice, theLastPostProcess->commandPool, queue /*postProcessQueue*/, "lastPostProcess_material");
		lastPostProcessMaterial->setShaderPaths(getShaderPath("postprocess.vert.spv"), getShaderPath("lastPostProcess.frag.spv"), "", "", "", "");
		lastPostProcessMaterial->setImageViews(sceneStage->outputImageView, verticalBlurPP2->outputImageView, depthStencil.view);
		lastPostProcessMaterial->createDescriptorSet();
		lastPostProcessMaterial->connectRenderPass(theLastPostProcess->renderPass);
		lastPostProcessMaterial->createGraphicsPipeline(glm::vec2(theLastPostProcess->width, theLastPostProcess->height), glm::vec2(0.0, 0.0));
	}
	void destroyPostProcessMaterials()
	{
		delete hdrHighlightMaterial;
		delete HBMaterial;
		delete VBMaterial;
		delete HBMaterial2;
		delete VBMaterial2;
		delete lastPostProcessMaterial;
	}
	
	// GOOD
	void LoadGlobalMaterials()
	{
		lightingMaterial->setDirectionalLights(&directionLights);
		lightingMaterial->LoadFromFilename(device, physicalDevice, sceneStage->commandPool, queue /*lightingQueue*/, "lighting_material");
		lightingMaterial->creatDirectionalLightBuffer();
		lightingMaterial->setShaderPaths(getShaderPath("lighting.vert.spv"), getShaderPath("lighting.frag.spv"), "", "", "", "");
		lightingMaterial->setGbuffers(&gBufferImageViews, depthStencil.view, standardShadow.outputImageView, vxgiPP->outputImageView);
		lightingMaterial->createDescriptorSet();
		lightingMaterial->connectRenderPass(sceneStage->renderPass);
		lightingMaterial->createGraphicsPipeline({ width, height });


		voxelConetracingMaterial->setDirectionalLights(&directionLights);
		voxelConetracingMaterial->LoadFromFilename(device, physicalDevice, vxgiPP->commandPool, queue /*postProcessQueue*/, "VXGI_material");
		voxelConetracingMaterial->creatDirectionalLightBuffer();
		voxelConetracingMaterial->setShaderPaths(getShaderPath("voxelConeTracing.vert.spv"), getShaderPath("voxelConeTracing.frag.spv"), "", "", "", "");
		voxelConetracingMaterial->setScreenScale(vxgiPP->getScreenScale());
		// Depends on voxelizer
		voxelConetracingMaterial->setImageViews(sceneStage->outputImageView, depthStencil.view, gBufferImageViews[NORMAL_COLOR], gBufferImageViews[SPECULAR_COLOR], &voxelizer.albedo3DImageViewSet, standardShadow.outputImageView);
		voxelConetracingMaterial->setBuffers(voxelizer.voxelInfoBuffer, lightingMaterial->shadowConstantBuffer);
		voxelConetracingMaterial->createDescriptorSet();
		voxelConetracingMaterial->connectRenderPass(vxgiPP->renderPass);
		voxelConetracingMaterial->createGraphicsPipeline(glm::vec2(vxgiPP->width, vxgiPP->height), glm::vec2(0.0, 0.0));


		debugDisplayMaterials.resize(NUM_DEBUGDISPLAY);
		for (size_t i = 0; i < NUM_DEBUGDISPLAY; i++)
		{
			debugDisplayMaterials[i] = new DebugDisplayMaterial;
			debugDisplayMaterials[i]->LoadFromFilename(device, physicalDevice, cmdPool /*deferredCommandPool*/, queue /*lightingQueue*/, "debugDisplay_material");
			debugDisplayMaterials[i]->setShaderPaths(getShaderPath("debug.vert.spv"), getShaderPath("debug" + std::to_string(i) + ".frag.spv"), "", "", "", "");
			debugDisplayMaterials[i]->setDubugBuffers(&gBufferImageViews, depthStencil.view, vxgiPP->outputImageView, standardShadow.outputImageView);
			debugDisplayMaterials[i]->createDescriptorSet();
			debugDisplayMaterials[i]->connectRenderPass(renderPass);
		}

		float debugWidth = std::ceilf(width * 0.25f);
		float debugHeight = std::ceilf(height * 0.25f);
		debugDisplayMaterials[0]->createGraphicsPipeline(glm::vec2(debugWidth, debugHeight), glm::vec2(0.0, 0.0));
		debugDisplayMaterials[1]->createGraphicsPipeline(glm::vec2(debugWidth, debugHeight), glm::vec2(debugWidth, 0.0));
		debugDisplayMaterials[2]->createGraphicsPipeline(glm::vec2(debugWidth, debugHeight), glm::vec2(debugWidth * 2.0, 0.0));
		debugDisplayMaterials[3]->createGraphicsPipeline(glm::vec2(debugWidth, debugHeight), glm::vec2(debugWidth * 3.0, 0.0));
		debugDisplayMaterials[4]->createGraphicsPipeline(glm::vec2(debugWidth, debugHeight), glm::vec2(0.0, debugHeight));
		debugDisplayMaterials[5]->createGraphicsPipeline(glm::vec2(debugWidth, debugHeight), glm::vec2(0.0, debugHeight * 2.0));
		debugDisplayMaterials[6]->createGraphicsPipeline(glm::vec2(debugWidth, debugHeight), glm::vec2(debugWidth * 3.0, debugHeight));
		debugDisplayMaterials[7]->createGraphicsPipeline(glm::vec2(debugWidth, debugHeight), glm::vec2(debugWidth * 3.0, debugHeight * 2.0));
		debugDisplayMaterials[8]->createGraphicsPipeline(glm::vec2(debugWidth, debugHeight), glm::vec2(0.0, debugHeight * 3.0));
		debugDisplayMaterials[9]->createGraphicsPipeline(glm::vec2(debugWidth, debugHeight), glm::vec2(debugWidth, debugHeight * 3.0));
		debugDisplayMaterials[10]->createGraphicsPipeline(glm::vec2(debugWidth, debugHeight), glm::vec2(debugWidth * 2.0, debugHeight * 3.0));
		debugDisplayMaterials[11]->createGraphicsPipeline(glm::vec2(debugWidth, debugHeight), glm::vec2(debugWidth * 3.0, debugHeight * 3.0));
	}
	void destroyGlobalMaterials()
	{
		delete voxelConetracingMaterial;  // default cleanup, TODO
		delete lightingMaterial;
		for (auto& debugMat : debugDisplayMaterials)
			delete debugMat;
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
		ObjectDrawMaterial* tempMat = new ObjectDrawMaterial;
		tempMat->LoadFromFilename(device, physicalDevice, cmdPool /*deferredCommandPool*/, queue /*objectDrawQueue*/, name);
		tempMat->addTexture(AssetDatabase::GetInstance()->LoadAsset<Texture>(albedo));
		tempMat->addTexture(AssetDatabase::GetInstance()->LoadAsset<Texture>(specular));
		tempMat->addTexture(AssetDatabase::GetInstance()->LoadAsset<Texture>(normal));
		tempMat->addTexture(AssetDatabase::GetInstance()->LoadAsset<Texture>(emissive));
		tempMat->setShaderPaths(getShaderPath("shader.vert.spv"), getShaderPath("shader.frag.spv"), "", "", "", "");
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

	void shadowSetup()
	{
		// setup shadow
		standardShadow.Initialize(device, physicalDevice, swapChain.surface, 1, queue /*TagQueue*/, &objectManager);
		//standardShadow.setExtent(16384, 4096);
		standardShadow.setExtent(SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT);
		standardShadow.createImages(VK_FORMAT_R16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		{
			//standardShadow.createCommandPool();
			standardShadow.commandPool = cmdPool;
		}
		standardShadow.createRenderPass();
		standardShadow.createDepthResources();
		standardShadow.createFramebuffer();
		standardShadow.createSemaphore();
	}

	void voxelizerSetup()
	{
		voxelizer.Initialize(device, physicalDevice, swapChain.surface, 1, uint32_t(floor(log2(VOXEL_SIZE))), glm::vec2(1.0, 1.0));
		voxelizer.createImages(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		{
			//voxelizer.createCommandPool();
			voxelizer.commandPool = cmdPool;
		}
		voxelizer.setQueue(queue /*objectDrawQueue*/, queue /*TagQueue*/, queue /*AllocationQueue*/, queue /*MipmapQueue*/);
		voxelizer.createRenderPass();
		voxelizer.createFramebuffer();
		voxelizer.createSemaphore();
	}

	void postprocessesSetup()
	{
		//[Stage] Post-process
		PostProcess* VXGIPostProcess = new PostProcess;
		VXGIPostProcess->Initialize(device, physicalDevice, swapChain.surface, width, height, 1, 1, glm::vec2(1.0f, 1.0f), false, DRAW_INDEX, 0, false, NULL);
		VXGIPostProcess->createImages(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//VXGIPostProcess->createCommandPool();
		VXGIPostProcess->commandPool = cmdPool;
		VXGIPostProcess->createRenderPass();
		VXGIPostProcess->createFramebuffer();
		VXGIPostProcess->createSemaphore();
		VXGIPostProcess->material = voxelConetracingMaterial = new VoxelConetracingMaterial;
		vxgiPP = VXGIPostProcess;

		PostProcess* SceneImageStage = new PostProcess;
		SceneImageStage->Initialize(device, physicalDevice, swapChain.surface, width, height, 1, 1, glm::vec2(1.0f, 1.0f), false, DRAW_INDEX, 0, false, NULL);
		SceneImageStage->createImages(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//SceneImageStage->createCommandPool();
		SceneImageStage->commandPool = cmdPool;
		SceneImageStage->createRenderPass();
		SceneImageStage->createFramebuffer();
		SceneImageStage->createSemaphore();
		SceneImageStage->material = lightingMaterial = new LightingMaterial;
		sceneStage = SceneImageStage;

		PostProcess* HDRHighlightPostProcess = new PostProcess;
		HDRHighlightPostProcess->Initialize(device, physicalDevice, swapChain.surface, width, height, 1, 1, glm::vec2(DOWNSAMPLING_BLOOM, DOWNSAMPLING_BLOOM), false, DRAW_INDEX, 0, false, NULL);
		HDRHighlightPostProcess->createImages(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//HDRHighlightPostProcess->createCommandPool();
		HDRHighlightPostProcess->commandPool = cmdPool;
		HDRHighlightPostProcess->createRenderPass();
		HDRHighlightPostProcess->createFramebuffer();
		HDRHighlightPostProcess->createSemaphore();
		HDRHighlightPostProcess->material = hdrHighlightMaterial = new HDRHighlightMaterial;
		hdrHighlightPP = HDRHighlightPostProcess;

		PostProcess* HorizontalBlurPostProcess = new PostProcess;
		HorizontalBlurPostProcess->Initialize(device, physicalDevice, swapChain.surface, width, height, 1, 1, glm::vec2(DOWNSAMPLING_BLOOM * 2.0f, DOWNSAMPLING_BLOOM * 2.0f), false, DRAW_INDEX, 0, false, NULL);
		HorizontalBlurPostProcess->createImages(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//HorizontalBlurPostProcess->createCommandPool();
		HorizontalBlurPostProcess->commandPool = cmdPool;
		HorizontalBlurPostProcess->createRenderPass();
		HorizontalBlurPostProcess->createFramebuffer();
		HorizontalBlurPostProcess->createSemaphore();
		HorizontalBlurPostProcess->material = HBMaterial = new BlurMaterial;
		horizontalBlurPP = HorizontalBlurPostProcess;

		PostProcess* VerticalBlurPostProcess = new PostProcess;
		VerticalBlurPostProcess->Initialize(device, physicalDevice, swapChain.surface, width, height, 1, 1, glm::vec2(DOWNSAMPLING_BLOOM * 4.0f, DOWNSAMPLING_BLOOM * 4.0f), false, DRAW_INDEX, 0, false, NULL);
		VerticalBlurPostProcess->createImages(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//VerticalBlurPostProcess->createCommandPool();
		VerticalBlurPostProcess->commandPool = cmdPool;
		VerticalBlurPostProcess->createRenderPass();
		VerticalBlurPostProcess->createFramebuffer();
		VerticalBlurPostProcess->createSemaphore();
		VerticalBlurPostProcess->material = VBMaterial = new BlurMaterial;
		verticalBlurPP = VerticalBlurPostProcess;

		PostProcess* HorizontalBlurPostProcess2 = new PostProcess;
		HorizontalBlurPostProcess2->Initialize(device, physicalDevice, swapChain.surface, width, height, 1, 1, glm::vec2(DOWNSAMPLING_BLOOM * 8.0f, DOWNSAMPLING_BLOOM * 8.0f), false, DRAW_INDEX, 0, false, NULL);
		HorizontalBlurPostProcess2->createImages(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//HorizontalBlurPostProcess2->createCommandPool();
		HorizontalBlurPostProcess2->commandPool = cmdPool;
		HorizontalBlurPostProcess2->createRenderPass();
		HorizontalBlurPostProcess2->createFramebuffer();
		HorizontalBlurPostProcess2->createSemaphore();
		HorizontalBlurPostProcess2->material = HBMaterial2 = new BlurMaterial;
		horizontalBlurPP2 = HorizontalBlurPostProcess2;

		PostProcess* VerticalBlurPostProcess2 = new PostProcess;
		VerticalBlurPostProcess2->Initialize(device, physicalDevice, swapChain.surface, width, height, 1, 1, glm::vec2(DOWNSAMPLING_BLOOM * 16.0f, DOWNSAMPLING_BLOOM * 16.0f), false, DRAW_INDEX, 0, false, NULL);
		VerticalBlurPostProcess2->createImages(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//VerticalBlurPostProcess2->createCommandPool();
		VerticalBlurPostProcess2->commandPool = cmdPool;
		VerticalBlurPostProcess2->createRenderPass();
		VerticalBlurPostProcess2->createFramebuffer();
		VerticalBlurPostProcess2->createSemaphore();
		VerticalBlurPostProcess2->material = VBMaterial2 = new BlurMaterial;
		verticalBlurPP2 = VerticalBlurPostProcess2;

		PostProcess* LastPostProcess = new PostProcess;  // Tone mapping
		LastPostProcess->Initialize(device, physicalDevice, swapChain.surface, width, height, 1, 1, glm::vec2(1.0f, 1.0f), false, DRAW_INDEX, 0, false, NULL);
		LastPostProcess->createImages(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//LastPostProcess->createCommandPool();
		LastPostProcess->commandPool = cmdPool;
		LastPostProcess->createRenderPass();
		LastPostProcess->createFramebuffer();
		LastPostProcess->createSemaphore();
		LastPostProcess->material = lastPostProcessMaterial = new LastPostProcessgMaterial;
		theLastPostProcess = LastPostProcess;  //!!! theLastPostProcess has to indicate the last post process, always !!!

		postProcessStages.push_back(VXGIPostProcess);            // Voxel GI
		postProcessStages.push_back(SceneImageStage);
		postProcessStages.push_back(HDRHighlightPostProcess);    // HDR highlight
		postProcessStages.push_back(HorizontalBlurPostProcess);  // Blurs
		postProcessStages.push_back(VerticalBlurPostProcess);
		postProcessStages.push_back(HorizontalBlurPostProcess2);
		postProcessStages.push_back(VerticalBlurPostProcess2);
		postProcessStages.push_back(LastPostProcess);
	}


	void draw()  // helper for render(), submitting to cmd queue
	{
		VulkanExampleBase::prepareFrame();  // acquire the next swapchain image

		VkSemaphore prevSemaphore = semaphores.presentComplete;
		VkSemaphore currentSemaphore;

		submitInfo.commandBufferCount = 1;

		//objectDrawQueue - Gbuffers for deferred rendering
		{
			currentSemaphore = objectDrawCompleteSemaphore;
			submitInfo.pWaitSemaphores = &prevSemaphore;
			submitInfo.pCommandBuffers = &deferredCommandBuffer;
			submitInfo.pSignalSemaphores = &currentSemaphore;
			VK_CHECK_RESULT(vkQueueSubmit(queue /*objectDrawQueue*/, 1, &submitInfo, VK_NULL_HANDLE))
			prevSemaphore = currentSemaphore;
		}

		//DrawShadow - generate shadow map
		{
			currentSemaphore = standardShadow.semaphore;
			submitInfo.pWaitSemaphores = &prevSemaphore;
			submitInfo.pCommandBuffers = &standardShadow.commandBuffer;
			submitInfo.pSignalSemaphores = &currentSemaphore;
			VK_CHECK_RESULT(vkQueueSubmit(queue /*standardShadow.queue*/, 1, &submitInfo, VK_NULL_HANDLE))
			prevSemaphore = currentSemaphore;
		}

		//postProcessQueue
		// 0. voxel cone tracing pass
		// 1. lighting pass
		// 2. HDR highlight pass
		// 3-6. hightlight blur pass
		// 7. tone mapping pass
		{
			//prevSemaphore = voxelizer.createMipmaps(prevSemaphore);
			int i = 0;
			for (auto& post : postProcessStages)
			{
				//if (i >= 2 && i++ <= 6) continue;

				currentSemaphore = post->postProcessSemaphore;
				submitInfo.pWaitSemaphores = &prevSemaphore;
				submitInfo.pCommandBuffers = &(post->commandBuffer);
				submitInfo.pSignalSemaphores = &currentSemaphore;
				VK_CHECK_RESULT(vkQueueSubmit(queue /*postProcess->material->queue*/, 1, &submitInfo, VK_NULL_HANDLE))
				prevSemaphore = currentSemaphore;
			}
		}

		//frameQueue
		{
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
		sleep(10);

        VulkanExampleBase::prepare();

		createSemaphores();
		createGbuffers();
		createSceneBuffer();
		createImageViews();

		shadowSetup();
		voxelizerSetup();
		postprocessesSetup();
		initLights();
		loadAssets();  // Textures + models

		LoadGlobalMaterials();
		LoadPostProcessMaterials();
		LoadFrameBufferMaterial();

		createDeferredRenderPass();
		createDeferredFramebuffer();
		for (const auto& mat : materialManager)  // Pipeline
		{
			mat->connectRenderPass(deferredRenderPass);
			mat->createGraphicsPipeline({width, height});
		}

		for (const auto& obj : objectManager)
		{
			obj->createShadowMaterial(
				standardShadow.commandPool, standardShadow.queue, standardShadow.renderPass,
				glm::vec2(standardShadow.Extent2D.width, standardShadow.Extent2D.height),
				glm::vec2(0.0, 0.0), lightingMaterial->shadowConstantBuffer); // !!!!!!!
		}

		buildCommandBuffers();  // override, build drawCmdBuffers (frame buffers)
		buildDeferredCommandBuffer();
		standardShadow.createCommandBuffers();
		voxelizer.createCommandBuffers();
		for (const auto& post : postProcessStages)
		{
			post->createCommandBuffers();
		}

		// Voxelization
		voxelizer.createMipmaps(voxelizer.createVoxels(camera, VK_NULL_HANDLE));
		
		prepared = true;
	}

	void render() override
	{
		if (!prepared)
			return;

		updateUniformBuffers(frameTimer);
		draw();

		//updateUniformBufferDeferredLights();
		
		//if (camera.updated) 
		//{
		//	updateUniformBufferOffscreen();
		//}
	}

	virtual void viewChanged() override
	{
		updateUniformBuffers(frameTimer);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
	{
		if (overlay->header("Settings")) {
			if (overlay->comboBox("Lighting", &drawMode, {"DI Only", "AO Only", "DI + GI", "DI + AO", "DI + GI + AO" }))
			{
				//updateUniformBufferDeferredLights();
			}

			if (overlay->checkBox("GBuffers view", &bGbufferView)) {
				//
			}
			if (overlay->checkBox("Rotate main light", &bRotateMainLight)) {
				//
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()
