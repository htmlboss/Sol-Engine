#pragma once

#include "ISystem.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>

struct GLFWwindow;

class RenderSystem : public ISystem {

public:
	explicit RenderSystem() = default;
	~RenderSystem() = default;	

	RenderSystem(const RenderSystem&) = delete;
	RenderSystem& operator=(const RenderSystem&) = delete;

	void init() override;
	void update(const float delta) override;
	void shutdown() override;
	void waitDeviceIdle() const;

private:
	// Helper structs to hold Vulkan setup info
	/***********************************************************************************/
	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;

		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};
	/***********************************************************************************/
	struct QueueFamilyIndices {
		int graphicsFamily = -1;
		int presentFamily = -1;

		auto isComplete() const noexcept {
			return graphicsFamily >= 0 && presentFamily >= 0;
		}
	};
	/***********************************************************************************/

	// Core Vulkan setup
	void createInstance();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();
	// Creates VMA
	// https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/quick_start.html
	void createMemoryAllocator();
	void createSwapChain();
	void createImageViews();
	void createRenderPass();
	void createDescriptorSetLayout();
	// Where shader objects are created and options set for Vertex layout, viewport, scissors, MSAA, etc.
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPools();
	// Setup and configure depth images for depth buffering
	void createDepthAttachment();
	void createTextureImage();
	void createTextureImageView();
	// The sampler is a distinct object that provides an interface to extract colors from a texture. 
	// It can be applied to any image you want, whether it is 1D, 2D or 3D. 
	// This is different from many older APIs, which combined texture images and filtering into a single state.
	void createTextureSampler();
	void createVertexBuffer();
	void createIndexBuffer();
	void createUniformBuffer();
	void createDescriptorPools();
	void createDescriptorSet();
	void createCommandBuffers();
	void createSemaphores();
	void cleanupSwapChain();
	// Called when the window resizes to recreate the swapchain and depth attachments.
	void recreateSwapChain();
	
	// Helper stuff
	bool isDeviceSuitable(const VkPhysicalDevice device);
	bool checkDeviceExtensionSupport(const VkPhysicalDevice device) const;
	std::uint32_t findMemoryType(const std::uint32_t typeFilter, const VkMemoryPropertyFlags& properties) const;
	QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice device) const;
	SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice device) const;
	std::vector<const char*> getRequiredExtensions() const;
	// Wrapper for creating and begin recording to a command buffer.
	VkCommandBuffer createAndBeginCommandBuffer(const VkCommandPool commandPool) const;
	// Wrapper for ending recording to command buffer and submits to graphics queue.
	void endAndSubmitCommandBuffer(const VkCommandPool commandPool, const VkCommandBuffer commandBuffer) const;
	// Helper function to create a Vulkan buffer (vertex, index, etc).
	// Returns a VmaAllocationInfo in case you want to do a persistent memory mapping:
	// https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/memory_mapping.html
	VmaAllocationInfo createBuffer(const VkDeviceSize size, const VkBufferUsageFlags usage, VkBuffer& buffer, VmaAllocation& allocation) const;
	// Helper function to copy a Vulkan buffer to a destination buffer.
	void copyBuffer(const VkBuffer src, const VkBuffer dest, const VkDeviceSize size) const;
	// Updates uniform buffer every frame before rendering.
	void updateUniformBuffer(const float dt) const;
	// Helper function to create a Vulkan image buffer.
	void createImage(const std::uint32_t width, const std::uint32_t height, const VkFormat format, const VkImageTiling tiling, const VkImageUsageFlags usage, VkImage& image, VmaAllocation& allocation) const;
	// Helper function to move an image into GPU memory.
	void transitionImageLayout(const VkImage image, const VkFormat format, const VkImageLayout oldLayout, const VkImageLayout newLayout) const;
	void copyBufferToImage(const VkBuffer buffer, const VkImage image, std::uint32_t width, std::uint32_t height);
	// Helper function to create a VkImageView (for swap chain or just texture images).
	VkImageView createImageView(const VkImage image, const VkFormat format, const VkImageAspectFlags aspectFlags) const;
	// Takes a list of candidate image formats in order from most desirable to least desirable, and checks which is the first one that is supported.
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, const VkImageTiling tiling, const VkFormatFeatureFlags features) const;
	// Helper function to select a format with a depth component that supports usage as depth attachment.
	VkFormat findDepthFormat() const;
	// Helper function that checks if a given image format contains a stencil component.
	bool hasStencilComponent(const VkFormat format) const;

	// Shader creation

	// Reads a compiled SPIR-V shader file from disk.
	std::vector<char> readShaderFile(const std::string_view filename) const;
	// Creates a Vulkan shader module from loaded shader file.
	VkShaderModule createShaderModule(const std::vector<char>& code) const;

	VkInstance m_instance;
	
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device;
	const std::vector<const char*> m_deviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	// Memory Allocation
	VmaAllocator m_allocator;
	VmaAllocation m_textureAllocation, m_depthAllocation, m_vertexBufferAllocation, m_indexBufferAllocation, m_uniformBufferAllocation;
	VmaAllocationInfo m_uniformBufferAllocInfo;

	VkQueue m_graphicsQueue, m_presentQueue;

	VkSurfaceKHR m_surface;

	VkSwapchainKHR m_swapChain;
	std::vector<VkImage> m_swapChainImages;
	std::vector<VkImageView> m_swapChainImageViews;
	std::vector<VkFramebuffer> m_swapChainFramebuffers;
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;

	VkRenderPass m_renderPass;
	VkDescriptorSetLayout m_descriptorSetLayout;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;

	VkCommandPool m_drawingCommandPool, m_memoryTransferCommandPool;
	std::vector<VkCommandBuffer> m_commandBuffers;

	VkSemaphore m_imageAvailableSemaphore;
	VkSemaphore m_renderFinishedSemaphore;

	VkImage m_textureImage, m_depthImage;
	VkImageView m_textureImageView, m_depthImageView;
	VkSampler m_textureSampler;
	VkBuffer m_vertexBuffer, m_indexBuffer, m_uniformBuffer;

	VkDescriptorPool m_descriptorPool;
	VkDescriptorSet m_descriptorSet;

/***********************************************************************************/
	// Debug stuff
#ifdef _DEBUG
	void createDebugCallback();
	bool checkValidationLayerSupport() const;

	VkDebugReportCallbackEXT m_debugCallback;
	const std::vector<const char*> m_validationLayers {
		"VK_LAYER_LUNARG_standard_validation"
	};
#endif
};