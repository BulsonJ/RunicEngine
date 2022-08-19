#pragma once

#include <glm.hpp>

#include <vector>
#include <optional>

namespace Runic
{
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec3 color;
		glm::vec2 uv;
		glm::vec3 tangent;
	};

	struct MeshDesc
	{
		typedef uint32_t Index;

		std::vector<Vertex> vertices;
		std::vector<Index> indices;

		bool hasIndices() const;

		static glm::vec3 CalculateSurfaceNormal(glm::vec3 pointA, glm::vec3 pointB, glm::vec3 pointC)
		{
			const glm::vec3 sideAB = pointB - pointA;
			const glm::vec3 sideAC = pointC - pointA;

			return glm::cross(sideAB, sideAC);
		}

		static Runic::MeshDesc GenerateTriangle();
		static Runic::MeshDesc GenerateQuad();
		static Runic::MeshDesc GenerateCube();
		static Runic::MeshDesc GenerateSkyboxCube();
		static Runic::MeshDesc GeneratePlane(int size);
	};
}