#pragma once

#include "Vertex.h"
#include "Texture.h"

#include <string_view>
#include <memory>

struct Mesh {
	Mesh(const std::vector<Vertex>& verts, const std::vector<std::uint32_t>& inds, const std::string_view imgpath);

	static std::shared_ptr<Mesh> loadModel(const std::string_view modelPath, const std::string_view texturePath);

	std::vector<Vertex> vertices;
	std::vector<std::uint32_t> indices;
	
	VkBuffer vertexBuffer, indexBuffer;
	VmaAllocation vertexBufferAllocation, indexBufferAllocation;
	Texture texture;
};

using MeshPtr = std::shared_ptr<Mesh>;