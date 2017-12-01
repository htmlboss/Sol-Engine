#pragma once

#include <vk_mem_alloc.h>

// Wrapper around the Vulkan physical and logical devices
class Device {

public:
	Device();

	void init(const VkInstance vkInstance);
	void shutdown() const;

private:
	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;

	bool isDeviceSuitable(const VkPhysicalDevice device) const;
};