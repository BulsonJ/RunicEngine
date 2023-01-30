#include "Runic/Graphics/ModelLoader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif

// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "tiny_gltf.h"

#include "Runic/Graphics/Renderer.h"
#include "Runic/Log.h"
#include "Runic/Graphics/Mesh.h"
#include "Runic/Graphics/Texture.h"

#include <unordered_map>
#include <gtc/type_ptr.hpp>

using namespace Runic;
using namespace tinygltf;

Runic::ModelLoader::ModelLoader(Renderer* rend) : m_rend(rend)
{

} 

std::optional<std::vector<RenderableComponent>> ModelLoader::LoadModelFromObj(const std::string& filename)
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
			const TextureHandle objectTextureHandle = m_rend->UploadTexture(objectTexture);
			loadedTextures[m] = objectTextureHandle;
			LOG_CORE_TRACE("Texture Uploaded: " + textureName);
		}
		if (materials[m].normal_texname != "" && materials[m].diffuse_texname != materials[m].ambient_texname && loadedNormalTextures.count(m) == 0)
		{
			Texture objectTexture;
			const std::string textureName = (directory + "/" + materials[m].ambient_texname);
			TextureUtil::LoadTextureFromFile(textureName.c_str(), { .format = TextureDesc::Format::NORMAL }, objectTexture);
			const TextureHandle objectTextureHandle = m_rend->UploadTexture(objectTexture);
			loadedNormalTextures[m] = objectTextureHandle;
			LOG_CORE_TRACE("Texture Uploaded: " + textureName);
		}
	}

	std::vector<RenderableComponent> newRenderObjects;

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
		RenderableComponent newRenderObject;
		newRenderObject.meshHandle = m_rend->UploadMesh(newMesh);
		newRenderObject.textureHandle = loadedTextures[shapes[s].mesh.material_ids[0]];
		newRenderObject.normalHandle = loadedNormalTextures[shapes[s].mesh.material_ids[0]];
		newRenderObjects.push_back(newRenderObject);
	}

	LOG_CORE_TRACE("Mesh Uploaded: " + filename);
	return newRenderObjects;
}

std::optional<std::vector<RenderableComponent>> Runic::ModelLoader::LoadModelFromGLTF(const std::string& filename)
{
	Model model;
	TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
	//bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]); // for binary glTF(.glb)

	if (!warn.empty())
	{
		printf("Warn: %s\n", warn.c_str());
	}

	if (!err.empty())
	{
		printf("Err: %s\n", err.c_str());
	}

	if (!ret)
	{
		printf("Failed to parse glTF\n");
		return std::nullopt;
	}


	const std::size_t directoryPos = filename.find_last_of("/");
	const std::string directory = filename.substr(0, directoryPos).c_str();

	std::vector<TextureHandle> loadedTextures;
	for (const tinygltf::Image& img : model.images)
	{
		Texture objectTexture{
			.texWidth = img.width,
			.texHeight = img.width,
			.texChannels = img.component,
		};
		const void* pixels = reinterpret_cast<const void*>(img.image.data());
		objectTexture.ptr[0] = (void*)pixels;
		//objectTexture.ptr[0] = imgPath.image.data();
		const TextureHandle objectTextureHandle = m_rend->UploadTexture(objectTexture);
		loadedTextures.push_back(objectTextureHandle);
		//LOG_CORE_TRACE("Texture Uploaded: " + textureName);
	}

	std::vector<RenderableComponent> newRenderObjects;
	tinygltf::Scene scene = model.scenes[0];
	for (const int node : scene.nodes)
	{
		Node currentNode = model.nodes[node];
		Mesh currentMesh = model.meshes[currentNode.mesh];

		for (const Primitive prim : currentMesh.primitives)
		{
			MeshDesc mesh;
			auto begin = prim.attributes.begin();
			auto end = prim.attributes.end();
			auto it = begin;

			auto posAttribute = prim.attributes.find("POSITION");
			const int vertexAttributeIndex = posAttribute->second;
			Accessor vertexAccessor = model.accessors[vertexAttributeIndex];
			mesh.vertices.assign(vertexAccessor.count, {});

			mesh.indices.assign(model.accessors[prim.indices].count, {});

			Accessor indexAccessor = model.accessors[prim.indices];
			BufferView indexBufferView = model.bufferViews[indexAccessor.bufferView];
			tinygltf::Buffer indexBuffer = model.buffers[indexBufferView.buffer];
			const unsigned short* indices = reinterpret_cast<const unsigned short*>(&indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset]);
			for (int i = 0; i < indexAccessor.count; ++i)
			{
				mesh.indices[i] = indices[i];
			}


			while (it != end)
			{

				Accessor currentAccessor = model.accessors[it->second];
				BufferView bufferView = model.bufferViews[currentAccessor.bufferView];
				tinygltf::Buffer currentBuffer = model.buffers[bufferView.buffer];
				
				const float* positions = reinterpret_cast<const float*>(&currentBuffer.data[bufferView.byteOffset + currentAccessor.byteOffset]);
				std::string attributeName = it->first;

				if (attributeName == "POSITION")
				{
					for (int i = 0; i < currentAccessor.count; ++i)
					{
						const int index = i * 3;
						mesh.vertices[i].position = glm::make_vec3(&positions[index]);

					}
				}
				else if (attributeName == "NORMAL") 
				{
					for (int i = 0; i < currentAccessor.count; ++i)
					{
						const int index = i * 3;
						mesh.vertices[i].normal = glm::make_vec3(&positions[index]);
					}
				}
				else if (attributeName == "TEXCOORD_0")
				{
					for (int i = 0; i < currentAccessor.count; ++i)
					{
						const int index = i * 2;
						mesh.vertices[i].uv = glm::make_vec2(&positions[index]);
					}
				}
				else if (attributeName == "TANGENT")
				{
					for (int i = 0; i < currentAccessor.count; ++i)
					{
						const int index = i * 4;
						mesh.vertices[i].tangent = glm::make_vec4(&positions[index]);
					}
				}
				it++;
			}

			Material modelMat = model.materials[prim.material];


			auto getTextureIndex = [=](const int materialImageIndex)->int {
				if (materialImageIndex < 0)
				{
					return -1;
				}
				return model.textures[materialImageIndex].source;
			};

			const int colIndex = getTextureIndex(modelMat.pbrMetallicRoughness.baseColorTexture.index);
			const int roughnessIndex = getTextureIndex(modelMat.pbrMetallicRoughness.metallicRoughnessTexture.index);
			const int normIndex = getTextureIndex(modelMat.normalTexture.index);
			const int emissiveIndex = getTextureIndex(modelMat.emissiveTexture.index);
			RenderableComponent newRenderObject;
			newRenderObject.meshHandle = m_rend->UploadMesh(mesh);
			newRenderObject.textureHandle = colIndex >= 0 ? loadedTextures[colIndex] : 0;
			newRenderObject.normalHandle = normIndex >= 0 ? loadedTextures[normIndex] : 0;
			newRenderObject.roughnessHandle = roughnessIndex >= 0 ? loadedTextures[roughnessIndex] : 0;
			newRenderObject.emissionHandle = emissiveIndex >= 0 ? loadedTextures[emissiveIndex] : 0;
			newRenderObjects.push_back(newRenderObject);
		}
	}



	return newRenderObjects;
}
