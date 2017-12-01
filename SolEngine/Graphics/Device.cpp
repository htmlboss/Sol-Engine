#include "Device.h"

#include <Logging/Log.h>
#include <set>

/***********************************************************************************/
Device::Device() : m_physicalDevice(VK_NULL_HANDLE) {
}

/***********************************************************************************/
void Device::init(const VkInstance& vkInstance, const VkSurfaceKHR& surface, VkQueue& graphicsQueue, VkQueue& presentQueue) {
	// Pick physical GPU with Vulkan support (if any)
	std::uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		LOG_CRITICAL("Failed to find GPUs with Vulkan support.");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());

	for (const auto& device : devices) {
		if (isDeviceSuitable(device, surface)) {
			m_physicalDevice = device;
			break;
		}
	}

	if (m_physicalDevice == VK_NULL_HANDLE) {
		LOG_CRITICAL("Failed to find GPUs with Vulkan support.");
	}

	// Create logical device
	const auto indices = findQueueFamilies(m_physicalDevice, surface);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	const std::set<int> uniqueQueueFamilies{ indices.graphicsFamily, indices.presentFamily };

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
	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
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

	vkGetDeviceQueue(m_device, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(m_device, indices.presentFamily, 0, &presentQueue);
}

/***********************************************************************************/
void Device::shutdown() const {
	vkDestroyDevice(m_device, nullptr);
}

/***********************************************************************************/
void Device::waitIdle() const {
	vkDeviceWaitIdle(m_device);
}

/***********************************************************************************/
bool Device::isDeviceSuitable(const VkPhysicalDevice device, const VkSurfaceKHR surface) const {
	const auto indices = findQueueFamilies(device, surface);

	const auto extensionsSupported = checkDeviceExtensionSupport(device);

	auto swapChainAdequate = false;
	if (extensionsSupported) {
		const auto swapChainSupport = querySwapChainSupport(device, surface);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	// Request hardware-supported graphics features (Anisotropic filtering, etc).
	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

/***********************************************************************************/
bool Device::checkDeviceExtensionSupport(const VkPhysicalDevice device) const {
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
Device::QueueFamilyIndices Device::findQueueFamilies(const VkPhysicalDevice device, const VkSurfaceKHR surface) const {
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
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

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
Device::SwapChainSupportDetails Device::querySwapChainSupport(const VkPhysicalDevice device, const VkSurfaceKHR surface) const {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	std::uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	std::uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}
