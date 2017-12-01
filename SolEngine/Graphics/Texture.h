#pragma once

#include <vk_mem_alloc.h>

#include <string_view>

// Wrapper around the Vulkan objects required to create 
// a image Texture that can be access in shaders.
struct Texture {
	explicit Texture(const std::string_view Path);

	void init(const VkDevice device);
	void destroy(const VkDevice device);

	const std::string_view path;
	int width, height, numChannels;

	VkImage image;
	VmaAllocation imageAllocation;
	VkImageView imageView;
};
