#pragma once

#include "Vertex.h"

#include <string_view>
#include <memory>

#include <vk_mem_alloc.h>

struct Mesh {
	Mesh(const std::vector<Vertex>& verts, const std::vector<std::uint32_t>& inds);

	static std::shared_ptr<Mesh> loadModel(const std::string_view modelPath, const std::string_view texturePath);

	std::vector<Vertex> vertices;
	std::vector<std::uint32_t> indices;
	
	VkBuffer vertexBuffer, indexBuffer;
	VmaAllocation vertexBufferAllocation, indexBufferAllocation;
};

using MeshPtr = std::shared_ptr<Mesh>;