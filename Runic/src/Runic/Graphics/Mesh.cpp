#include "Runic/Graphics/Mesh.h"

#include <iostream>
#include <array>
#include <tiny_obj_loader.h>

#include "Runic/Log.h"

Runic::MeshDesc Runic::MeshDesc::GenerateTriangle() {
	Runic::MeshDesc triangleMesh;

	triangleMesh.vertices.resize(3);

	triangleMesh.vertices[0].position = { 0.0f,-0.5f, 0.0f };
	triangleMesh.vertices[1].position = { 0.5f, 0.5f, 0.0f };
	triangleMesh.vertices[2].position = { -0.5f, 0.5f, 0.0f };

	triangleMesh.vertices[0].normal = { 0.f, 1.f, 0.0f };
	triangleMesh.vertices[1].normal = { 0.f, 1.f, 0.0f };
	triangleMesh.vertices[2].normal = { 0.f, 1.f, 0.0f };

	triangleMesh.vertices[0].color = { 1.f, 0.f, 0.0f };
	triangleMesh.vertices[1].color = { 0.f, 1.f, 0.0f };
	triangleMesh.vertices[2].color = { 0.f, 0.f, 1.0f };

	triangleMesh.vertices[0].uv = { 0.f, 1.f};
	triangleMesh.vertices[1].uv = { 1.f, 0.f};
	triangleMesh.vertices[2].uv = { 0.f, 0.f};

	return triangleMesh;
}

Runic::MeshDesc Runic::MeshDesc::GenerateQuad() {
	Runic::MeshDesc mesh;

	mesh.vertices.resize(4);

	mesh.vertices[0].position = { -1.0f, 1.0f, 0.0f };
	mesh.vertices[1].position = { 1.0f, 1.0f, 0.0f };
	mesh.vertices[2].position = { -1.0f,-1.0f, 0.0f };
	mesh.vertices[3].position = { 1.0f,-1.0f, 0.0f };

	mesh.indices.resize(6);
	mesh.indices[0] = 0;
	mesh.indices[1] = 1;
	mesh.indices[2] = 2;
	mesh.indices[3] = 1;
	mesh.indices[4] = 3;
	mesh.indices[5] = 2;

	mesh.vertices[0].normal = { 0.f, 1.f, 0.0f };
	mesh.vertices[1].normal = { 0.f, 1.f, 0.0f };
	mesh.vertices[2].normal = { 0.f, 1.f, 0.0f };
	mesh.vertices[3].normal = { 0.f, 1.f, 0.0f };

	mesh.vertices[0].color = { 0.f, 1.f, 0.0f };
	mesh.vertices[1].color = { 0.f, 1.f, 0.0f };
	mesh.vertices[2].color = { 0.f, 1.f, 0.0f };
	mesh.vertices[3].color = { 0.f, 1.f, 0.0f };

	mesh.vertices[0].uv = { 0.f, 1.f };
	mesh.vertices[1].uv = { 1.f, 1.f };
	mesh.vertices[2].uv = { 0.f, 0.f };
	mesh.vertices[3].uv = { 1.f, 0.f };

	return mesh;
}

Runic::MeshDesc Runic::MeshDesc::GenerateCube() {
	Runic::MeshDesc mesh;

	mesh.vertices.resize(4*6);

	// Z- face
	mesh.vertices[0].position  = { -0.5f, 0.5f, -0.5f };
	mesh.vertices[1].position  = {  0.5f, 0.5f, -0.5f };
	mesh.vertices[2].position  = { -0.5f,-0.5f, -0.5f };
	mesh.vertices[3].position  = {  0.5f,-0.5f, -0.5f };

	mesh.vertices[0].normal = { 0.f, 0.0f, -1.0f };
	mesh.vertices[1].normal = { 0.f, 0.0f, -1.0f };
	mesh.vertices[2].normal = { 0.f, 0.0f, -1.0f };
	mesh.vertices[3].normal = { 0.f, 0.0f, -1.0f };

	// Z+ face
	mesh.vertices[4].position = { -0.5f, 0.5f, 0.5f };
	mesh.vertices[5].position = { 0.5f, 0.5f, 0.5f };
	mesh.vertices[6].position = { -0.5f,-0.5f, 0.5f };
	mesh.vertices[7].position = { 0.5f,-0.5f, 0.5f };

	mesh.vertices[4].normal = { 0.f, 0.0f, 1.0f};
	mesh.vertices[5].normal = { 0.f, 0.0f, 1.0f };
	mesh.vertices[6].normal = { 0.f, 0.0f, 1.0f };
	mesh.vertices[7].normal = { 0.f, 0.0f, 1.0f  };

	// Bottom Face
	mesh.vertices[8].position = { -0.5f, -0.5f, 0.5f };
	mesh.vertices[9].position = { 0.5f, -0.5f, 0.5f };
	mesh.vertices[10].position = { -0.5f, -0.5f,-0.5f };
	mesh.vertices[11].position = { 0.5f, -0.5f,-0.5f };

	mesh.vertices[8].normal = { 0.f, -1.f, 0.0f };
	mesh.vertices[9].normal = { 0.f, -1.f, 0.0f };
	mesh.vertices[10].normal = { 0.f, -1.f, 0.0f };
	mesh.vertices[11].normal = { 0.f, -1.f, 0.0f };

	// Top Face
	mesh.vertices[12].position = { -0.5f, 0.5f, 0.5f };
	mesh.vertices[13].position = {  0.5f, 0.5f, 0.5f };
	mesh.vertices[14].position = { -0.5f, 0.5f,-0.5f };
	mesh.vertices[15].position = {  0.5f, 0.5f,-0.5f };

	mesh.vertices[12].normal = { 0.f, 1.f, 0.0f };
	mesh.vertices[13].normal = { 0.f, 1.f, 0.0f };
	mesh.vertices[14].normal = { 0.f, 1.f, 0.0f };
	mesh.vertices[15].normal = { 0.f, 1.f, 0.0f };

	// X- face
	mesh.vertices[16].position = { -0.5f,  -0.5f, 0.5f };
	mesh.vertices[17].position = { -0.5f,   0.5f, 0.5f };
	mesh.vertices[18].position = { -0.5f,  -0.5f,-0.5f };
	mesh.vertices[19].position = { -0.5f,   0.5f,-0.5f };

	mesh.vertices[16].normal = { -1.f, 0.0f, 0.0f };
	mesh.vertices[17].normal = { -1.f, 0.0f, 0.0f };
	mesh.vertices[18].normal = { -1.f, 0.0f, 0.0f };
	mesh.vertices[19].normal = { -1.f, 0.0f, 0.0f };
	// X+ face
	mesh.vertices[20].position = { 0.5f,  -0.5f, 0.5f };
	mesh.vertices[21].position = { 0.5f,   0.5f, 0.5f };
	mesh.vertices[22].position = { 0.5f,  -0.5f,-0.5f };
	mesh.vertices[23].position = { 0.5f,   0.5f,-0.5f };

	mesh.vertices[20].normal = { 1.f, 0.0f, 0.0f };
	mesh.vertices[21].normal = { 1.f, 0.0f, 0.0f };
	mesh.vertices[22].normal = { 1.f, 0.0f, 0.0f };
	mesh.vertices[23].normal = { 1.f, 0.0f, 0.0f };

	mesh.indices.resize(6*6);
	for (int i = 0; i < mesh.indices.size()/6; ++i) {
		int triIndex = i * 6;
		int vertIndex = i * 4;
		mesh.indices[triIndex + 0] = vertIndex + 0;
		mesh.indices[triIndex + 1] = vertIndex + 1;
		mesh.indices[triIndex + 2] = vertIndex + 2;
		mesh.indices[triIndex + 3] = vertIndex + 1;
		mesh.indices[triIndex + 4] = vertIndex + 3;
		mesh.indices[triIndex + 5] = vertIndex + 2;
	}

	for (int i = 0; i < mesh.vertices.size() / 4; ++i) {
		int vertIndex = i * 4;

		mesh.vertices[vertIndex + 0].color = { 1.f, 1.f, 1.f };
		mesh.vertices[vertIndex + 1].color = { 1.f, 1.f, 1.f };
		mesh.vertices[vertIndex + 2].color = { 1.f, 1.f, 1.f };
		mesh.vertices[vertIndex + 3].color = { 1.f, 1.f, 1.f };
					   
		mesh.vertices[vertIndex + 0].uv = { 0.f, 1.f };
		mesh.vertices[vertIndex + 1].uv = { 1.f, 1.f };
		mesh.vertices[vertIndex + 2].uv = { 0.f, 0.f };
		mesh.vertices[vertIndex + 3].uv = { 1.f, 0.f };
	}

	return mesh;
}

Runic::MeshDesc Runic::MeshDesc::GenerateSkyboxCube() {
	Runic::MeshDesc mesh;

	mesh.vertices.resize(4 * 6);

	// Z- face
	mesh.vertices[0].position = { -1.0f, 1.0f, -1.0f };
	mesh.vertices[1].position = { 1.0f, 1.0f, -1.0f };
	mesh.vertices[2].position = { -1.0f,-1.0f, -1.0f };
	mesh.vertices[3].position = { 1.0f,-1.0f, -1.0f };

	mesh.vertices[0].normal = { 0.f, 0.0f, -1.0f };
	mesh.vertices[1].normal = { 0.f, 0.0f, -1.0f };
	mesh.vertices[2].normal = { 0.f, 0.0f, -1.0f };
	mesh.vertices[3].normal = { 0.f, 0.0f, -1.0f };

	// Z+ face
	mesh.vertices[4].position = { -1.0f, 1.0f, 1.0f };
	mesh.vertices[5].position = { 1.0f, 1.0f, 1.0f };
	mesh.vertices[6].position = { -1.0f,-1.0f, 1.0f };
	mesh.vertices[7].position = { 1.0f,-1.0f, 1.0f };

	mesh.vertices[4].normal = { 0.f, 0.0f, 1.0f };
	mesh.vertices[5].normal = { 0.f, 0.0f, 1.0f };
	mesh.vertices[6].normal = { 0.f, 0.0f, 1.0f };
	mesh.vertices[7].normal = { 0.f, 0.0f, 1.0f };

	// Bottom Face
	mesh.vertices[8].position = { -1.0f, -1.0f, 1.0f };
	mesh.vertices[9].position = { 1.0f, -1.0f, 1.0f };
	mesh.vertices[10].position = { -1.0f, -1.0f,-1.0f };
	mesh.vertices[11].position = { 1.0f, -1.0f,-1.0f };

	mesh.vertices[8].normal = { 0.f, -1.f, 0.0f };
	mesh.vertices[9].normal = { 0.f, -1.f, 0.0f };
	mesh.vertices[10].normal = { 0.f, -1.f, 0.0f };
	mesh.vertices[11].normal = { 0.f, -1.f, 0.0f };

	// Top Face
	mesh.vertices[12].position = { -1.0f, 1.0f, 1.0f };
	mesh.vertices[13].position = { 1.0f, 1.0f, 1.0f };
	mesh.vertices[14].position = { -1.0f, 1.0f,-1.0f };
	mesh.vertices[15].position = { 1.0f, 1.0f,-1.0f };

	mesh.vertices[12].normal = { 0.f, 1.f, 0.0f };
	mesh.vertices[13].normal = { 0.f, 1.f, 0.0f };
	mesh.vertices[14].normal = { 0.f, 1.f, 0.0f };
	mesh.vertices[15].normal = { 0.f, 1.f, 0.0f };

	// X- face
	mesh.vertices[16].position = { -1.0f,  -1.0f, 1.0f };
	mesh.vertices[17].position = { -1.0f,   1.0f, 1.0f };
	mesh.vertices[18].position = { -1.0f,  -1.0f,-1.0f };
	mesh.vertices[19].position = { -1.0f,   1.0f,-1.0f };

	mesh.vertices[16].normal = { -1.f, 0.0f, 0.0f };
	mesh.vertices[17].normal = { -1.f, 0.0f, 0.0f };
	mesh.vertices[18].normal = { -1.f, 0.0f, 0.0f };
	mesh.vertices[19].normal = { -1.f, 0.0f, 0.0f };
	// X+ face
	mesh.vertices[20].position = { 1.0f,  -1.0f, 1.0f };
	mesh.vertices[21].position = { 1.0f,   1.0f, 1.0f };
	mesh.vertices[22].position = { 1.0f,  -1.0f,-1.0f };
	mesh.vertices[23].position = { 1.0f,   1.0f,-1.0f };

	mesh.vertices[20].normal = { 1.f, 0.0f, 0.0f };
	mesh.vertices[21].normal = { 1.f, 0.0f, 0.0f };
	mesh.vertices[22].normal = { 1.f, 0.0f, 0.0f };
	mesh.vertices[23].normal = { 1.f, 0.0f, 0.0f };

	mesh.indices.resize(6 * 6);
	for (int i = 0; i < mesh.indices.size() / 6; ++i) {
		int triIndex = i * 6;
		int vertIndex = i * 4;
		mesh.indices[triIndex + 0] = vertIndex + 0;
		mesh.indices[triIndex + 1] = vertIndex + 1;
		mesh.indices[triIndex + 2] = vertIndex + 2;
		mesh.indices[triIndex + 3] = vertIndex + 1;
		mesh.indices[triIndex + 4] = vertIndex + 3;
		mesh.indices[triIndex + 5] = vertIndex + 2;
	}

	for (int i = 0; i < mesh.vertices.size() / 4; ++i) {
		int vertIndex = i * 4;

		mesh.vertices[vertIndex + 0].color = { 1.f, 1.f, 1.f };
		mesh.vertices[vertIndex + 1].color = { 1.f, 1.f, 1.f };
		mesh.vertices[vertIndex + 2].color = { 1.f, 1.f, 1.f };
		mesh.vertices[vertIndex + 3].color = { 1.f, 1.f, 1.f };

		mesh.vertices[vertIndex + 0].uv = { 0.f, 1.f };
		mesh.vertices[vertIndex + 1].uv = { 1.f, 1.f };
		mesh.vertices[vertIndex + 2].uv = { 0.f, 0.f };
		mesh.vertices[vertIndex + 3].uv = { 1.f, 0.f };
	}

	return mesh;
}

Runic::MeshDesc Runic::MeshDesc::GeneratePlane(int size) {
	Runic::MeshDesc mesh;

	const int meshSize = size + 1;
	mesh.vertices.resize(meshSize * meshSize);

	const float halfPos = size / 2.f;

	int verts = 0;
	for (int z = 0; z < meshSize; z++) {
		for (int x = 0; x < meshSize; x++) {
			mesh.vertices[verts].position = { static_cast<float>(x) - halfPos, 0.f, z - halfPos };

			mesh.vertices[verts].normal = { 0.f, 1.f, 0.0f };

			mesh.vertices[verts].color = { 1.f, 1.f, 1.0f };

			mesh.vertices[verts].uv = { static_cast<float>(x) / size , static_cast<float>(z) / size };
			verts++;
		}
	}

	mesh.indices.resize(size * size * 6);
	int lastTri = 0;
	int vertex = 0;
	for (int z = 0; z < size; ++z) {
		for (int x = 0; x < size; ++x) {
			mesh.indices[lastTri + 0] = vertex + 0;
			mesh.indices[lastTri + 1] = vertex + meshSize;
			mesh.indices[lastTri + 2] = vertex + meshSize + 1;
			mesh.indices[lastTri + 3] = vertex + 0;
			mesh.indices[lastTri + 4] = vertex + meshSize + 1;
			mesh.indices[lastTri + 5] = vertex + 1;
			lastTri += 6;
			vertex++;
		}
		vertex++;
	}

	return mesh;
}

bool Runic::MeshDesc::hasIndices() const {
	if (this->indices.size() > 0) {
		return true;
	}
	return false;
}

bool Runic::MeshDesc::loadFromObj(const char* filename)
{
	//attrib will contain the vertex arrays of the file
	tinyobj::attrib_t attrib;
	//shapes contains the info for each separate object in the file
	std::vector<tinyobj::shape_t> shapes;
	//materials contains the information about the material of each shape, but we won't use it.
	std::vector<tinyobj::material_t> materials;

	//error and warning output from the load function
	std::string warn;
	std::string err;

	//load the OBJ file
	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, nullptr);
	//make sure to output the warnings to the console, in case there are issues with the file
	if (!warn.empty())
	{
		LOG_CORE_WARN(warn);
	}
	//if we have any error, print it to the console, and break the mesh loading.
	//This happens if the file can't be found or is malformed
	if (!err.empty())
	{
		std::cerr << err << std::endl;
		return false;
	}

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++)
	{
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
		{

			//hardcode loading to triangles
			int fv = 3;

			// Loop over vertices in the face.
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


				vertices.push_back(new_vert);
			}
			index_offset += fv;
		}
	}

	return true;
}