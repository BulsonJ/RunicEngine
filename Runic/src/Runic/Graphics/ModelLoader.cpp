#include "Runic/Graphics/ModelLoader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "Runic/Graphics/Renderer.h"
#include "Runic/Log.h"
#include "Runic/Graphics/Mesh.h"
#include "Runic/Graphics/Texture.h"

#include <unordered_map>

using namespace Runic;

Runic::ModelLoader::ModelLoader(Renderer* rend) : m_rend(rend)
{

} 

std::optional<std::vector<RenderObject>> ModelLoader::LoadModelFromObj(const std::string& filename)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	const std::size_t directoryPos = filename.find_last_of("/");
	const std::string directory = filename.substr(0, directoryPos).c_str();
	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str(), directory.c_str());
	if (!warn.empty())
	{
		LOG_CORE_WARN(warn);
	}
	if (!err.empty())
	{
		LOG_CORE_WARN(err);
		return std::nullopt;
	}

	std::unordered_map<size_t, TextureHandle> loadedTextures;
	std::unordered_map<size_t, TextureHandle> loadedNormalTextures;
	for (size_t m = 0; m < materials.size(); m++)
	{
		if (materials[m].diffuse_texname != "" && loadedTextures.count(m) == 0)
		{
			Texture objectTexture;
			const std::string textureName = (directory + "/" + materials[m].diffuse_texname);
			TextureUtil::LoadTextureFromFile(textureName.c_str(), { .format = TextureDesc::Format::DEFAULT }, objectTexture);
			const TextureHandle objectTextureHandle = m_rend->uploadTexture(objectTexture);
			loadedTextures[m] = objectTextureHandle;
			LOG_CORE_TRACE("Texture Uploaded: " + textureName);
		}
		if (materials[m].normal_texname != "" && materials[m].diffuse_texname != materials[m].ambient_texname && loadedNormalTextures.count(m) == 0)
		{
			Texture objectTexture;
			const std::string textureName = (directory + "/" + materials[m].ambient_texname);
			TextureUtil::LoadTextureFromFile(textureName.c_str(), { .format = TextureDesc::Format::NORMAL }, objectTexture);
			const TextureHandle objectTextureHandle = m_rend->uploadTexture(objectTexture);
			loadedNormalTextures[m] = objectTextureHandle;
			LOG_CORE_TRACE("Texture Uploaded: " + textureName);
		}
	}

	std::vector<RenderObject> newRenderObjects;

	for (size_t s = 0; s < shapes.size(); s++)
	{
		MeshDesc newMesh;
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
		{
			int fv = 3;
			for (size_t v = 0; v < fv; v++)
			{
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

				//vertex position
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				//vertex normal
				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

				//copy it into our vertex
				Vertex new_vert;
				new_vert.position.x = vx;
				new_vert.position.y = vy;
				new_vert.position.z = vz;

				new_vert.normal.x = nx;
				new_vert.normal.y = ny;
				new_vert.normal.z = nz;

				//we are setting the vertex color as the vertex normal. This is just for display purposes
				new_vert.color = new_vert.normal;

				tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
				tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

				new_vert.uv.x = ux;
				new_vert.uv.y = 1 - uy;

				newMesh.vertices.push_back(new_vert);
			}
			index_offset += fv;
		}
		RenderObject newRenderObject{
			.meshHandle = m_rend->uploadMesh(newMesh),
			.textureHandle = loadedTextures[shapes[s].mesh.material_ids[0]],
			.normalHandle = loadedNormalTextures[shapes[s].mesh.material_ids[0]],
		};
		newRenderObjects.push_back(newRenderObject);
	}

	LOG_CORE_TRACE("Mesh Uploaded: " + filename);
	return newRenderObjects;
}
