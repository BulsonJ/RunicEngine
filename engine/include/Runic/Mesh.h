#pragma once

#include <glm.hpp>

#include <vector>
#include <optional>

namespace RenderableTypes
{
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec3 color;
		glm::vec2 uv;
	};

	struct MeshDesc
	{
		typedef uint32_t Index;

		std::vector<Vertex> vertices;
		std::vector<Index> indices;

		bool hasIndices() const;
		bool loadFromObj(const char* filename);

		static glm::vec3 CalculateSurfaceNormal(glm::vec3 pointA, glm::vec3 pointB, glm::vec3 pointC)
		{
			const glm::vec3 sideAB = pointB - pointA;
			const glm::vec3 sideAC = pointC - pointA;

			return glm::cross(sideAB, sideAC);
		}

		static RenderableTypes::MeshDesc GenerateTriangle();
		static RenderableTypes::MeshDesc GenerateQuad();
		static RenderableTypes::MeshDesc GenerateCube();
		static RenderableTypes::MeshDesc GenerateSkyboxCube();
		static RenderableTypes::MeshDesc GeneratePlane(int size);
	};
}