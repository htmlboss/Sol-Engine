#define VMA_IMPLEMENTATION
#include "RenderSystem.h"

#include "WindowSystem.h"
#include "Input.h"
#include "Graphics/Vertex.h"
#include "Logging/Log.h"

#include <GLFW/GLFW3.h>

#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <set>
#include <fstream>

/***********************************************************************************/
#ifdef _DEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugReportFlagsEXT       flags,
	VkDebugReportObjectTypeEXT  objectType,
	uint64_t                    object,
	size_t                      location,
	int32_t                     messageCode,
	const char*                 pLayerPrefix,
	const char*                 pMessage,
	void*                       pUserData) {

	LOG_ERROR(pMessage);

	return VK_FALSE;
}

VkResult CreateDebugReportCallbackEXT(
	VkInstance									instance,
	const VkDebugReportCallbackCreateInfoEXT*	pCreateInfo,
	const VkAllocationCallbacks*				pAllocator,
	VkDebugReportCallbackEXT*					pCallback) {
	
	const auto func = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}
	
	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugReportCallbackEXT(
	VkInstance						instance, 
	VkDebugReportCallbackEXT		callback, 
	const VkAllocationCallbacks*	pAllocator) {
	
	const auto func = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));
	if (func != nullptr) {
		func(instance, callback, pAllocator);
	}
}
#endif

/***********************************************************************************/
VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
		return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	return availableFormats[0];
}

/***********************************************************************************/
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
		if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			bestMode = availablePresentMode;
		}
	}

	return bestMode;
}

/***********************************************************************************/
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const std::size_t width, const std::size_t height) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	
	VkExtent2D actualExtent { width, height };
	actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

	return actualExtent;
}

/***********************************************************************************/
void RenderSystem::addMeshes(const std::vector<MeshPtr>& meshes) {
	m_meshes = meshes;
}

/***********************************************************************************/
void RenderSystem::init() {
	createInstance();
#ifdef _DEBUG
	createDebugCallback();
#endif
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createMemoryAllocator();
	createSwapChain();
	createImageViews();
	createRenderPass();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createCommandPools();
	createDepthAttachment();
	createFramebuffers(); // Needs to be called after depth attachment is created.
	createTextureImage();
	createTextureImageView();
	createTextureSampler();
	prepareMeshes(); // TODO: Process mesh vector
	//createVertexBuffer();
	//createIndexBuffer();
	createUniformBuffer();
	createDescriptorPools();
	createDescriptorSet();
	createCommandBuffers();
	createSemaphores();
}

/***********************************************************************************/
void RenderSystem::update(const float delta) {

	updateUniformBuffer(delta);

	// Window size changed.
	if (Input::GetInstance().ShouldResize() && 
		Input::GetInstance().GetWidth() > 0 && 
		Input::GetInstance().GetHeight() > 0) {
		recreateSwapChain();
	}

	std::uint32_t imageIndex;
	auto result = vkAcquireNextImageKHR(m_device, m_swapChain, std::numeric_limits<std::uint64_t>::max(), 
		m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapChain();
		return;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		spdlog::get("console")->error("Failed to acquire swap chain image.");
		std::abort();
	}

	VkSubmitInfo submitInfo {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	const VkSemaphore waitSemaphores[] { m_imageAvailableSemaphore };
	const VkPipelineStageFlags waitStages[] { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];

	const VkSemaphore signalSemaphores[] { m_renderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		spdlog::get("console")->error("Failed to submit draw command buffer!");
		std::abort();
	}

	VkPresentInfoKHR presentInfo {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	const VkSwapchainKHR swapChains[] { m_swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr; // Optional

	result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
	
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS) {
		spdlog::get("console")->error("Failed to present swap chain image.");
		std::abort();
	}

	vkQueueWaitIdle(m_presentQueue);
}

/***********************************************************************************/
void RenderSystem::shutdown() {
	cleanupSwapChain();

	// Cleanup any image textures
	vkDestroySampler(m_device, m_textureSampler, nullptr);
	vkDestroyImageView(m_device, m_textureImageView, nullptr);
	// VMA cleans object and memory allocation all-in-one
	vmaDestroyImage(m_allocator, m_textureImage, m_textureAllocation);

	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

	// Cleanup mesh resources
	for (auto& mesh : m_meshes) {
		vmaDestroyBuffer(m_allocator, mesh->indexBuffer, mesh->indexBufferAllocation);
		vmaDestroyBuffer(m_allocator, mesh->vertexBuffer, mesh->vertexBufferAllocation);
	}
	
	vmaDestroyBuffer(m_allocator, m_uniformBuffer, m_uniformBufferAllocation);

	vkDestroySemaphore(m_device, m_renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);

	vkDestroyCommandPool(m_device, m_memoryTransferCommandPool, nullptr);
	vkDestroyCommandPool(m_device, m_drawingCommandPool, nullptr);

	// Clean up Vulkan Memory Allocator
	vmaDestroyAllocator(m_allocator);

	vkDestroyDevice(m_device, nullptr);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
#ifdef _DEBUG
	DestroyDebugReportCallbackEXT(m_instance, m_debugCallback, nullptr);
#endif
	// Finally wreck the place
	vkDestroyInstance(m_instance, nullptr);
}

/***********************************************************************************/
void RenderSystem::waitDeviceIdle() const {
	vkDeviceWaitIdle(m_device);
}

/***********************************************************************************/
void RenderSystem::createInstance() {
#ifdef _DEBUG
	if (!checkValidationLayerSupport()) {
		LOG_CRITICAL("Validation layers requested, but not available.");
	}
#endif

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instanceCreateInfo {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;

	const auto extensions = getRequiredExtensions();
	instanceCreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
#ifdef _DEBUG
	instanceCreateInfo.enabledLayerCount = static_cast<std::uint32_t>(m_validationLayers.size());
	instanceCreateInfo.ppEnabledLayerNames = m_validationLayers.data();
#else
	instanceCreateInfo.enabledLayerCount = 0;
#endif

	if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create Vulkan instance.");
	}
}

/***********************************************************************************/
void RenderSystem::createSurface() {
	if (glfwCreateWindowSurface(m_instance, WindowSystem::getWindowPtr(), nullptr, &m_surface) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create Vulkan surface.");
	}
}

/***********************************************************************************/
void RenderSystem::pickPhysicalDevice() {
	std::uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		LOG_CRITICAL("Failed to find GPUs with Vulkan support.");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			m_physicalDevice = device;
			break;
		}
	}

	if (m_physicalDevice == VK_NULL_HANDLE) {
		LOG_CRITICAL("Failed to find GPUs with Vulkan support.");
	}
}

/***********************************************************************************/
void RenderSystem::createLogicalDevice() {
	const auto indices = findQueueFamilies(m_physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	const std::set<int> uniqueQueueFamilies { indices.graphicsFamily, indices.presentFamily };

	const auto queuePriority = 1.0f;
	for (auto queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Enable hardware features
	VkPhysicalDeviceFeatures deviceFeatures {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<std::uint32_t>(m_deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();
#ifdef _DEBUG
	createInfo.enabledLayerCount = static_cast<std::uint32_t>(m_validationLayers.size());
	createInfo.ppEnabledLayerNames = m_validationLayers.data();
#else
	createInfo.enabledLayerCount = 0;
#endif

	if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create logical device.");
	}

	vkGetDeviceQueue(m_device, indices.graphicsFamily, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device, indices.presentFamily, 0, &m_presentQueue);
}

/***********************************************************************************/
void RenderSystem::createMemoryAllocator() {
	VmaAllocatorCreateInfo allocatorInfo {};
	allocatorInfo.physicalDevice = m_physicalDevice;
	allocatorInfo.device = m_device;

	if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create Vulkan Memory Allocator.");
	}
}

/***********************************************************************************/
void RenderSystem::createSwapChain() {
	const auto swapChainSupport = querySwapChainSupport(m_physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, Input::GetInstance().GetWidth(), Input::GetInstance().GetHeight());

	std::uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const auto indices = findQueueFamilies(m_physicalDevice);
	const std::uint32_t queueFamilyIndices[] { static_cast<std::uint32_t>(indices.graphicsFamily), static_cast<std::uint32_t>(indices.presentFamily) };

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create swap chain.");
	}


	vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
	m_swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

	m_swapChainImageFormat = surfaceFormat.format;
	m_swapChainExtent = extent;
}

/***********************************************************************************/
void RenderSystem::createImageViews() {
	m_swapChainImageViews.resize(m_swapChainImages.size());

	for (auto i = 0; i < m_swapChainImages.size(); ++i) {
		m_swapChainImageViews[i] = createImageView(m_swapChainImages[i], m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

/***********************************************************************************/
void RenderSystem::createRenderPass() {
	// Colour attachment
	VkAttachmentDescription colorAttachment {};
	colorAttachment.format = m_swapChainImageFormat; // Should always match swap chain format
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Since we have no MSAA
	// For colour and depth data
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear the values to a constant at the start
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Rendered contents will be stored in memory and can be read later
	// Stencil data only
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Contents of the framebuffer will be undefined after the rendering operation
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Images to be presented in the swap chain

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth attachment
	VkAttachmentDescription depthAttachment {};
	depthAttachment.format = findDepthFormat();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // No MSAA
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Dont care about previous depth contents
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	const std::array<VkAttachmentDescription, 2> attachments {colorAttachment, depthAttachment};
	VkRenderPassCreateInfo renderPassInfo {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
	renderPassInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create render pass!");
	}
}

/***********************************************************************************/
void RenderSystem::createDescriptorSetLayout() {
	// For MVP ubo
	VkDescriptorSetLayoutBinding uboLayoutBinding {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.pImmutableSamplers = nullptr;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// For image sampler
	VkDescriptorSetLayoutBinding samplerLayoutBinding {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	const std::array<VkDescriptorSetLayoutBinding, 2> bindings { uboLayoutBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create descriptor set layout.");
	}

}

/***********************************************************************************/
void RenderSystem::createGraphicsPipeline() {
	const auto vertShaderCode = readShaderFile("Data/Shaders/vert.spv");
	const auto fragShaderCode = readShaderFile("Data/Shaders/frag.spv");

	const VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
	const VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

	// TODO: Put into static shader class?
	VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	const VkPipelineShaderStageCreateInfo shaderStages[] { vertShaderStageInfo, fragShaderStageInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
	const auto bindingDescription = Vertex::getBindingDescription();
	const auto attributeDescriptions = Vertex::getAttributeDescriptions();
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	// Vertex format
	VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_swapChainExtent.width);
	viewport.height = static_cast<float>(m_swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor {};
	scissor.offset = { 0, 0 };
	scissor.extent = m_swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE; // Only keep fragments that fall within the specified depth range.
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE; // Stencil test
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1; // # of descriptors
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;

	if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create pipeline layout.");
	}

	// Now actually create the damn pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.renderPass = m_renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create graphics pipeline.");
	}

	// Cleanup
	vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
	vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
}

/***********************************************************************************/
void RenderSystem::createFramebuffers() {
	m_swapChainFramebuffers.resize(m_swapChainImageViews.size());
	
	for (auto i = 0; i < m_swapChainImageViews.size(); ++i) {
		const std::array<VkImageView, 2> attachments {
			m_swapChainImageViews[i],
			m_depthImageView
		};

		VkFramebufferCreateInfo framebufferInfo {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_renderPass;
		framebufferInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = m_swapChainExtent.width;
		framebufferInfo.height = m_swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
			LOG_CRITICAL("Failed to create framebuffer.");
		}
	}
}

/***********************************************************************************/
void RenderSystem::createCommandPools() {
	// Drawing Command Pool
	const auto queueFamilyIndices = findQueueFamilies(m_physicalDevice);

	VkCommandPoolCreateInfo drawingPoolInfo {};
	drawingPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	drawingPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
	drawingPoolInfo.flags = 0; // Optional

	if (vkCreateCommandPool(m_device, &drawingPoolInfo, nullptr, &m_drawingCommandPool) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create command pool.");
	}

	// Memory Transfer Pool (for short-lived buffer creation)
	VkCommandPoolCreateInfo memoryTransferPoolInfo{};
	memoryTransferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	memoryTransferPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
	memoryTransferPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	if (vkCreateCommandPool(m_device, &memoryTransferPoolInfo, nullptr, &m_memoryTransferCommandPool) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create memory transfer command pool.");
	}
}

/***********************************************************************************/
void RenderSystem::createDepthAttachment() {
	const auto depthFormat = findDepthFormat();

	createImage(m_swapChainExtent.width, m_swapChainExtent.height, depthFormat, 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
		m_depthImage, 
		m_depthAllocation);

	m_depthImageView = createImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	// Use a pipeline barrier because the transition to suitable depth format only needs to happen once
	transitionImageLayout(m_depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

/***********************************************************************************/
void RenderSystem::createTextureImage() {
	int texWidth, texHeight, texChannels;
	auto* pixels = stbi_load("Data/Textures/03e.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) {
		LOG_CRITICAL("Failed to load image.");
	}

	const VkDeviceSize imageSize = texWidth * texHeight * 4;
	VkBuffer stagingBuffer;
	VmaAllocation allocation;
	const auto allocInfo = createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		stagingBuffer, 
		allocation);

	std::memcpy(allocInfo.pMappedData, pixels, imageSize);

	stbi_image_free(pixels);

	createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
		m_textureImage, 
		m_textureAllocation);

	transitionImageLayout(m_textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, m_textureImage, static_cast<std::uint32_t>(texWidth), static_cast<std::uint32_t>(texHeight));
	// One more transition so the shader can read from it
	transitionImageLayout(m_textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vmaDestroyBuffer(m_allocator, stagingBuffer, allocation);
}

/***********************************************************************************/
void RenderSystem::createTextureImageView() {
	m_textureImageView = createImageView(m_textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}

/***********************************************************************************/
void RenderSystem::createTextureSampler() {
	VkSamplerCreateInfo samplerInfo {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create texture sampler.");
	}

}

/***********************************************************************************/
void RenderSystem::prepareMeshes() {
	for (auto& mesh : m_meshes) {
		createVertexBuffer(mesh);
		createIndexBuffer(mesh);
	}
}

/***********************************************************************************/
void RenderSystem::createVertexBuffer(MeshPtr& mesh) {
	
	const VkDeviceSize bufferSize = sizeof(mesh->vertices[0]) * mesh->vertices.size();
	VkBuffer stagingBuffer;
	VmaAllocation allocation;
	const auto allocInfo = createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		stagingBuffer, 
		allocation);

	std::memcpy(allocInfo.pMappedData, mesh->vertices.data(), static_cast<std::size_t>(bufferSize));

	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
		mesh->vertexBuffer, 
		mesh->vertexBufferAllocation);

	copyBuffer(stagingBuffer, mesh->vertexBuffer, bufferSize);

	// Cleanup
	vmaDestroyBuffer(m_allocator, stagingBuffer, allocation);
}

/***********************************************************************************/
void RenderSystem::createIndexBuffer(MeshPtr& mesh) {
	const VkDeviceSize bufferSize = sizeof(mesh->indices[0]) * mesh->indices.size();

	VkBuffer stagingBuffer;
	VmaAllocation allocation;
	const auto allocInfo = createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		stagingBuffer, 
		allocation);

	std::memcpy(allocInfo.pMappedData, mesh->indices.data(), static_cast<std::size_t>(bufferSize));

	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
		mesh->indexBuffer, mesh->indexBufferAllocation);

	copyBuffer(stagingBuffer, mesh->indexBuffer, bufferSize);


	vmaDestroyBuffer(m_allocator, stagingBuffer, allocation);
}

/***********************************************************************************/
void RenderSystem::createUniformBuffer() {
	const VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	m_uniformBufferAllocInfo = createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
		m_uniformBuffer, 
		m_uniformBufferAllocation);
}

/***********************************************************************************/
void RenderSystem::createDescriptorPools() {
	std::array<VkDescriptorPoolSize, 2> poolSizes {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1;

	if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create descriptor pool.");
	}
}

/***********************************************************************************/
void RenderSystem::createDescriptorSet() {
	const VkDescriptorSetLayout layouts[] { m_descriptorSetLayout };
	
	VkDescriptorSetAllocateInfo allocInfo {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to allocate descriptor set.");
	}

	// UBO
	VkDescriptorBufferInfo bufferInfo {};
	bufferInfo.buffer = m_uniformBuffer;
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(UniformBufferObject);
	
	// For image texture
	VkDescriptorImageInfo imageInfo {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = m_textureImageView;
	imageInfo.sampler = m_textureSampler;

	std::array<VkWriteDescriptorSet, 2> descriptorWrites {};

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = m_descriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pBufferInfo = &bufferInfo;

	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = m_descriptorSet;
	descriptorWrites[1].dstBinding = 1;
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[1].descriptorCount = 1;
	descriptorWrites[1].pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(m_device, static_cast<std::uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

/***********************************************************************************/
void RenderSystem::createCommandBuffers() {
	m_commandBuffers.resize(m_swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_drawingCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Can be submitted to a queue for execution, but cannot be called from other command buffers.
	allocInfo.commandBufferCount = static_cast<std::uint32_t>(m_commandBuffers.size());

	if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
		LOG_ERROR("Failed to allocate command buffers.");
	}

	for (auto i = 0; i < m_commandBuffers.size(); ++i) {
		VkCommandBufferBeginInfo beginInfo {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo);

		VkRenderPassBeginInfo renderPassInfo {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_renderPass;
		renderPassInfo.framebuffer = m_swapChainFramebuffers[i];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_swapChainExtent;

		// Specify clear values for colour attachment, and depth/stencil.
		std::array<VkClearValue, 2> clearValues;
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 }; // Clear to farthest possible depth (1.0)
		renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

		// Bind Vertex Buffer
		const VkBuffer vertexBuffers[] { m_vertexBuffer };
		const VkDeviceSize offsets[] { 0 };
		vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(m_commandBuffers[i], m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		// Bind UBO
		vkCmdBindDescriptorSets(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

		vkCmdDrawIndexed(m_commandBuffers[i], static_cast<std::uint32_t>(mindices.size()), 1, 0, 0, 0);

		vkCmdEndRenderPass(m_commandBuffers[i]);

		if (vkEndCommandBuffer(m_commandBuffers[i]) != VK_SUCCESS) {
			LOG_CRITICAL("Failed to record command buffer.");
		}
	}
}

/***********************************************************************************/
void RenderSystem::createSemaphores() {
	VkSemaphoreCreateInfo semaphoreInfo {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphore) != VK_SUCCESS) {

		LOG_CRITICAL("Failed to create swapchain semaphores.");
	}
}

/***********************************************************************************/
void RenderSystem::cleanupSwapChain() {
	// Cleanup depth buffer
	vkDestroyImageView(m_device, m_depthImageView, nullptr);
	vmaDestroyImage(m_allocator, m_depthImage, m_depthAllocation);

	for (const auto& fb : m_swapChainFramebuffers) {
		vkDestroyFramebuffer(m_device, fb, nullptr);
	}

	vkFreeCommandBuffers(m_device, m_drawingCommandPool, static_cast<std::uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());

	vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_device, m_renderPass, nullptr);

	for (const auto& view : m_swapChainImageViews) {
		vkDestroyImageView(m_device, view, nullptr);
	}

	// Wreck the actual swapchain
	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
}

/***********************************************************************************/
void RenderSystem::recreateSwapChain() {
	vkDeviceWaitIdle(m_device);

	cleanupSwapChain();
	
	createSwapChain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createDepthAttachment();
	createFramebuffers();
	createCommandBuffers();
}

/***********************************************************************************/
bool RenderSystem::isDeviceSuitable(const VkPhysicalDevice device) {
	const auto indices = findQueueFamilies(device);

	const auto extensionsSupported = checkDeviceExtensionSupport(device);

	auto swapChainAdequate = false;
	if (extensionsSupported) {
		const auto swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}


	// Request hardware-supported graphics features (Anisotropic filtering, etc).
	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

/***********************************************************************************/
bool RenderSystem::checkDeviceExtensionSupport(const VkPhysicalDevice device) const {
	std::uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

/***********************************************************************************/
std::uint32_t RenderSystem::findMemoryType(const std::uint32_t typeFilter, const VkMemoryPropertyFlags& properties) const {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

	for (auto i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	LOG_CRITICAL("Failed to find suitable memory type.");
}

/***********************************************************************************/
RenderSystem::QueueFamilyIndices RenderSystem::findQueueFamilies(const VkPhysicalDevice device) const {
	QueueFamilyIndices indices;

	std::uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);

		if (queueFamily.queueCount > 0 && presentSupport) {
			indices.presentFamily = i;
		}

		if (indices.isComplete()) {
			break;
		}

		++i;
	}

	return indices;
}

/***********************************************************************************/
RenderSystem::SwapChainSupportDetails RenderSystem::querySwapChainSupport(const VkPhysicalDevice device) const {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

/***********************************************************************************/
std::vector<const char*> RenderSystem::getRequiredExtensions() const {
	std::vector<const char*> extensions;

	unsigned int glfwExtensionCount = 0;
	const auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	for (unsigned int i = 0; i < glfwExtensionCount; i++) {
		extensions.push_back(glfwExtensions[i]);
	}

#ifdef _DEBUG
	extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

	return extensions;
}

/***********************************************************************************/
VkCommandBuffer RenderSystem::createAndBeginCommandBuffer(const VkCommandPool commandPool) const {
	VkCommandBufferAllocateInfo allocInfo {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

/***********************************************************************************/
void RenderSystem::endAndSubmitCommandBuffer(const VkCommandPool commandPool, const VkCommandBuffer commandBuffer) const {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_graphicsQueue);

	vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
}

/***********************************************************************************/
VmaAllocationInfo RenderSystem::createBuffer(const VkDeviceSize size, const VkBufferUsageFlags usage, VkBuffer& buffer, VmaAllocation& allocation) const {
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocCreateInfo {};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VmaAllocationInfo allocInfo;

	if (vmaCreateBuffer(m_allocator, &bufferInfo, &allocCreateInfo, &buffer, &allocation, &allocInfo) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create buffer.");
	}

	return allocInfo;
}

/***********************************************************************************/
void RenderSystem::copyBuffer(const VkBuffer src, const VkBuffer dest, const VkDeviceSize size) const {
	const auto cmdBuffer = createAndBeginCommandBuffer(m_memoryTransferCommandPool);

	VkBufferCopy copyRegion {};
	copyRegion.size = size;
	vkCmdCopyBuffer(cmdBuffer, src, dest, 1, &copyRegion);
	
	endAndSubmitCommandBuffer(m_memoryTransferCommandPool, cmdBuffer);
}

/***********************************************************************************/
void RenderSystem::updateUniformBuffer(const float dt) const {
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

	UniformBufferObject ubo {};
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f), m_swapChainExtent.width / static_cast<float>(m_swapChainExtent.height), 0.1f, 10.0f);
	ubo.proj[1][1] *= -1; // Prevent image from being rendered upside down

	std::memcpy(m_uniformBufferAllocInfo.pMappedData, &ubo, sizeof(ubo));
}

/***********************************************************************************/
void RenderSystem::createImage(const std::uint32_t width, const std::uint32_t height, const VkFormat format, const VkImageTiling tiling, const VkImageUsageFlags usage, VkImage& image, VmaAllocation& allocation) const {
	VkImageCreateInfo imageInfo {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
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

	VmaAllocationCreateInfo allocInfo {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	
	if (vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create image.");
	}
}

/***********************************************************************************/
void RenderSystem::transitionImageLayout(const VkImage image, const VkFormat format,  const VkImageLayout oldLayout, const VkImageLayout newLayout) const {
	const auto commandBuffer = createAndBeginCommandBuffer(m_memoryTransferCommandPool);

	VkImageMemoryBarrier barrier {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	// Used to transfer ownership of a queue
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.image = image;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	// Since we're throwing a depth + stencil images into the mix we need to use the right subresource object
	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (hasStencilComponent(format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}
	
	// Handle 2 possible transition states:
	// Undefined to transfer destination: transfer writes that don't need to wait on anything
	// Transfer destination to shader reading: shader reads should wait on transfer writes, 
	// specifically the shader reads in the fragment shader,
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
	else {
		LOG_CRITICAL("Unsupported layout transition.");
	}


	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	endAndSubmitCommandBuffer(m_memoryTransferCommandPool, commandBuffer);
}

/***********************************************************************************/
void RenderSystem::copyBufferToImage(const VkBuffer buffer, const VkImage image, std::uint32_t width, std::uint32_t height) {
	const auto commandBuffer = createAndBeginCommandBuffer(m_memoryTransferCommandPool);
	
	VkBufferImageCopy region {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(
		commandBuffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	endAndSubmitCommandBuffer(m_memoryTransferCommandPool, commandBuffer);
}

/***********************************************************************************/
VkImageView RenderSystem::createImageView(const VkImage image, const VkFormat format, const VkImageAspectFlags aspectFlags) const {
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
	if (vkCreateImageView(m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create texture image view.");
	}

	return imageView;
}

/***********************************************************************************/
VkFormat RenderSystem::findSupportedFormat(const std::vector<VkFormat>& candidates, const VkImageTiling tiling, const VkFormatFeatureFlags features) const {
	for (const auto& format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

		// Use cases that are supported with linear tiling
		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		// Use cases that are supported with optimal tiling
		if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	LOG_CRITICAL("Failed to find supported image format.");
}

/***********************************************************************************/
VkFormat RenderSystem::findDepthFormat() const {
	return findSupportedFormat(
	{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

/***********************************************************************************/
bool RenderSystem::hasStencilComponent(const VkFormat format) const {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

/***********************************************************************************/
std::vector<char> RenderSystem::readShaderFile(const std::string_view filename) const {
	std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		LOG_ERROR("Failed to open file: ");
		LOG_CRITICAL(filename.data());
	}

	const auto fileSize = static_cast<std::size_t>(file.tellg());
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

/***********************************************************************************/
VkShaderModule RenderSystem::createShaderModule(const std::vector<char>& code) const {
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create shader module!");
	}

	return shaderModule;
}

/***********************************************************************************/
#ifdef _DEBUG
void RenderSystem::createDebugCallback() {
	VkDebugReportCallbackCreateInfoEXT createInfo {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
	createInfo.pfnCallback = debugCallback;

	if (CreateDebugReportCallbackEXT(m_instance, &createInfo, nullptr, &m_debugCallback) != VK_SUCCESS) {
		LOG_CRITICAL("Failed to create debug callback.");
	}
}

bool RenderSystem::checkValidationLayerSupport() const {
	std::uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layer : m_validationLayers) {
		auto layerFound = false;

		for (const auto& layerProperty : availableLayers) {
			if (std::strcmp(layer, layerProperty.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

#endif