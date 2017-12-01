#pragma once

#include <vk_mem_alloc.h>

#include <vector>

// Wrapper around the Vulkan physical and logical devices
class Device {

public:
	Device();

	void init(const VkInstance& vkInstance, const VkSurfaceKHR& surface, VkQueue& graphicsQueue, VkQueue& presentQueue);
	void shutdown() const;

	void waitIdle() const;

	auto getQueueFamiles(const VkSurfaceKHR& surface) const { return findQueueFamilies(m_physicalDevice, surface); }
	auto checkSwapChainSupport(const VkSurfaceKHR& surface) const { return querySwapChainSupport(m_physicalDevice, surface); }
	auto getPhysicalDevice() const noexcept { return m_physicalDevice; }
	auto getDevice() const noexcept { return m_device; }

private:
	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;

	const std::vector<const char*> m_deviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

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

	bool isDeviceSuitable(const VkPhysicalDevice device, const VkSurfaceKHR surface) const;
	bool checkDeviceExtensionSupport(const VkPhysicalDevice device) const;
	QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice device, const VkSurfaceKHR surface) const;
	SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice device, const VkSurfaceKHR surface) const;

#ifdef _DEBUG
	const std::vector<const char*> m_validationLayers {
		"VK_LAYER_LUNARG_standard_validation"
	};
#endif
};