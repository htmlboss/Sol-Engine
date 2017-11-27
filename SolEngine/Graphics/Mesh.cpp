#include "Mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "Log/Log.h"

#include <unordered_map>

/***********************************************************************************/
Mesh::Mesh(const std::vector<Vertex>& verts, const std::vector<std::uint32_t>& inds) : vertices(verts), indices(inds) {

}

/***********************************************************************************/
MeshPtr Mesh::loadModel(const std::string_view modelPath, const std::string_view texturePath) {
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, modelPath.data())) {
		LOG_CRITICAL(err);
	}

	std::vector<Vertex> vertices;
	std::vector<std::uint32_t> indices;
	// For removing duplicate vertices
	std::unordered_map<Vertex, std::uint32_t> uniqueVertices {};
	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			const Vertex vertex(
			{	
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2] 
			},
			{1.0f, 1.0f, 1.0f}, // Color
			{
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // Flip origin to top-left
			}
			);

			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<std::uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}
	}

	return std::make_shared<Mesh>(vertices, indices);
}
