#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/gtx/hash.hpp>

#include <array>
#include <vector>

// Describes a vertex in Vulkan
struct Vertex {
	Vertex(const glm::vec3& position, const glm::vec3& col, const glm::vec2& texcoords) : pos(position), color(col), texCoord(texcoords) {}

	auto operator==(const Vertex& rhs) const {
		return pos == rhs.pos && color == rhs.color && texCoord == rhs.texCoord;
	}

#ifdef VULKAN_H_
	
	// Describes at which rate to load data from memory
	static auto getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription {};
		
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	// How to extract a vertex attribute from a chunk of vertex data
	static auto getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions {};

		// Position
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		// Color
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		// TexCoords
		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}
#endif

	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;
};

namespace std {
	template<> 
	struct hash<Vertex> {
		auto operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}

// Temp junk
struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};