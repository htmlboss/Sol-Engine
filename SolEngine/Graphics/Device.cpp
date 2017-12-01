#include "Device.h"

#include <Logging/Log.h>

/***********************************************************************************/
Device::Device() : m_physicalDevice(VK_NULL_HANDLE) {
}

/***********************************************************************************/
void Device::init(const VkInstance vkInstance) {
	// Pick physical GPU with Vulkan support (if any)
	std::uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		LOG_CRITICAL("Failed to find GPUs with Vulkan support.");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());

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
void Device::shutdown() const {
	vkDestroyDevice(m_device, nullptr);
}

/***********************************************************************************/
bool Device::isDeviceSuitable(const VkPhysicalDevice device) const {
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
