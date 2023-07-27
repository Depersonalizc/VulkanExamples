#pragma once
#if 0

//#include "VulkanDebug.h"
#include "VulkanQueue.h"
//#include "VulkanSwapChain.h"

#include "Vertex.h"
#include "PostProcess.h"
#include "Shadow.h"
#include "Voxelization.h"

#include "../assets/Material.h"
#include "../assets/Geometry.h"
#include "../actors/Object.h"
#include "../actors/Camera.h"
#include "../actors/Light.h"

#include <sstream>



//static bool leftMouseDown = false;
//static bool rightMouseDown = false;
//static bool middleMouseDown = false;
//
//static double previousX = 0.0;
//static double previousY = 0.0;
//
//static double oldTime = 0.0;
//static double currentTime = 0.0;
//static int fps = 0;
//static int fpstracker = 0;
//
//static std::chrono::time_point<std::chrono::steady_clock> startTime;
//static std::chrono::time_point<std::chrono::steady_clock> _oldTime;
//
//static double totalTime;
//static double deltaTime;

static float mainLightAngle = 1.0f;

static std::string convertToString(int number)
{
	std::stringstream ss;
	ss << number;
	return ss.str();
}

static std::string convertToString(double number)
{
	std::stringstream ss;
	ss << number;
	return ss.str();
}

static CameraActor camera;

class VulkanApp
{
public:
	VulkanApp();
	~VulkanApp();

	//bool bRenderToHmd = false;
	//std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	//long long frameIndex = 0;
	//VkSemaphore mipMapStartSemaphore;

	void initVulkan();
	//void initWindow();

	//void createInstance();

	//bool checkValidationLayerSupport();
	//std::vector<const char*> getRequiredExtensions();
	//void setupDebugCallback();

	//void createSurface();  // handled by swapchain

	//void pickPhysicalDevice();
	//bool isDeviceSuitable(VkPhysicalDevice device);
	//bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	//void createLogicalDevice();
	
	//QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

	//SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	//VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	//VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);
	//VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	
	//void createSwapChain();
	void reCreateSwapChain();  // also creates
	void cleanUpSwapChain();
	void cleanUpSwapChainAtResize();

	/***DONE***/ void createImageViews();  // GBuffer image views
	//void createSwapChainImageViews();

	//void createFramebuffers();
	//void createFrameBufferRenderPass();

	//void createFrameBufferCommandPool();
	/***DONE, todo drawUI***/void createFrameBufferCommandBuffers();

	//void createFramebufferDescriptorSetLayout();
	//void createFramebufferDescriptorPool();
	//void createFramebufferDescriptorSet();

	/***DONE***/ void createGbuffers();
	/***DONE***/ void createSceneBuffer();

	/***DONE***/ void createDeferredFramebuffer();
	/***DONE***/ void createDeferredRenderPass();

	//void createDeferredCommandPool();
	/***DONE: buildDeferredCommandBuffer***/ void createDeferredCommandBuffers();

	/***DONE: draw()***/ void drawFrame(float deltaTime);

	//void createSemaphores();

	/***DONE***/ void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	/***DONE***/ uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);  // Helper for createImage

	/***DONE***/ VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

	/***DONE***/ void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandPool commandPool);
	/***DONE***/ VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);  // helper for transitionImageLayout
	/***DONE***/ void endSingleTimeCommands(VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue queue);  // helper for transitionImageLayout
	//bool hasStencilComponent(VkFormat format);  // helper for transitionImageLayout

	//void createDepthResources();
	//VkFormat findDepthFormat();
	//VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);


	/***DONE***/ void run();
	
	/***DONE***/ void updateUniformBuffers(float deltaTime);

	/***DONE***/ void mainLoop();

	void cleanUp();

	//void getAsynckeyState();

	//VkDevice getDevice()
	//{
	//	return device;
	//}
	
	// Assets
	/***DONE***/ void LoadTexture(std::string path);
	/***DONE***/ void LoadTextures();

	/***DONE***/ void LoadGlobalMaterials(PostProcess* vxgiPostProcess);
	/***DONE***/ void LoadObjectMaterials();
	/***DONE***/ void LoadObjectMaterial(std::string name, std::string albedo, std::string specular, std::string normal, std::string emissive);
	/***DONE***/ void ConnectSponzaMaterials(Object* sponza);

	/***DONE, todo refactor Voxelizer***/ void LoadObjects();

	// Misc
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location,	int32_t code, const char* layerPrefix, const char* msg,	void* userData);
	
	/***DONE***/ void switchTheLastPostProcess(unsigned int from, unsigned int to)
	{
		if (postProcessStages.size() > to)
		{
			if (theLastPostProcess == postProcessStages[to])
				theLastPostProcess = postProcessStages[from];
			else
				theLastPostProcess = postProcessStages[to];
		}
	}

	/***DONE***/ void swingMainLight()
	{
		SwingXAxisDirectionalLight(directionLights[0].lightInfo, 1.0f, mainLightAngle, 0.5f);
	}

	/***DONE***/ void updateDrawMode()
	{
		void* data;
		vkMapMemory(device, lightingMaterial->optionBufferMemory, 0, sizeof(uint32_t), 0, &data);
		memcpy(data, &drawMode, sizeof(uint32_t));
		vkUnmapMemory(device, lightingMaterial->optionBufferMemory);
	}

	//bool bIsRightEyeDrawing;

//private:
	//GLFWwindow* window;
	//GLFWmonitor* primaryMonitor;

	//VkInstance instance;
	//VkDebugReportCallbackEXT callback;

	//This object will be implicitly destroyed
	//VkPhysicalDevice physicalDevice;
	//VkDevice device;

	// Taken care of by the VulkanExampleBase::frameBuffers, swapChain, currentBuffer(index)
	//VkSurfaceKHR surface;
	//VkSwapchainKHR swapChain;
	//std::vector<VkImage> swapChainImages;
	//std::vector<VkImageView> swapChainImageViews;
	//std::vector<VkFramebuffer> swapChainFramebuffers;
	// Taken care of by VulkanExampleBase::frameBuffers?
	//VkFormat swapChainImageFormat;
	//VkExtent2D swapChainExtent;

	VkQueue objectDrawQueue;
	VkQueue TagQueue;
	VkQueue AllocationQueue;
	VkQueue MipmapQueue;
	VkQueue lightingQueue;
	VkQueue postProcessQueue;
	VkQueue presentQueue;

	// Taken care of by VulkanExampleBase::depthStencil
	//VkImage depthImage;
	//VkDeviceMemory depthImageMemory;
	//VkImageView depthImageView;


	
	//VkRenderPass frameBufferRenderPass;
	//VkCommandPool frameBufferCommandPool;                    // same as VulkanExample::vulkanDevice::commandPool
	//std::vector<VkCommandBuffer> frameBufferCommandBuffers;  // same as VulkanExample::drawCmdBuffers
	//std::vector<VkCommandBuffer> frameBufferCommandBuffers2;  // VR, right eye

	//VkDescriptorSetLayout descriptorSetLayout;
	//VkDescriptorPool descriptorPool;
	//VkDescriptorSet descriptorSet;

	StandardShadow standardShadow;
	Voxelization voxelizator;
	std::vector<DirectionalLight> directionLights;

	PostProcess* sceneStage;  //for FrameRender
	PostProcess* theLastPostProcess;
	std::vector<PostProcess*> postProcessStages;



	VoxelConetracingMaterial* voxelConetracingMaterial;
	LightingMaterial* lightingMaterial;

	HDRHighlightMaterial* hdrHighlightMaterial;

	BlurMaterial *HBMaterial;
	BlurMaterial *VBMaterial;

	BlurMaterial *HBMaterial2;
	BlurMaterial *VBMaterial2;

	ComputeBlurMaterial *compHBMaterial;
	ComputeBlurMaterial *compVBMaterial;

	ComputeBlurMaterial *compHBMaterial2;
	ComputeBlurMaterial *compVBMaterial2;

	singleTriangular* offScreenPlane;
	singleTriangular* offScreenPlaneforPostProcess;
	singleQuadral* debugDisplayPlane;

	std::vector<DebugDisplayMaterial*> debugDisplayMaterials;
	LastPostProcessgMaterial* lastPostProcessMaterial;
	VoxelRenderMaterial* voxelRenderMaterial;
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
	
};

#endif