#pragma once

#include <imgui.h>
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/sinks/ostream_sink.h"
#include "glm.hpp"

namespace Runic
{
	namespace Editor
	{
		extern ImTextureID ViewportTexture;
		extern ImTextureID ViewportDepthTexture;

		extern glm::vec4* lightDirection;
		extern glm::vec4* lightColor;
		extern glm::vec4* lightAmbientColor;

		void DrawEditor();

		void DrawViewport();
		void DrawViewportDepth();
		void DrawSceneGraph();
		void DrawProperties();
		void DrawLog();
	};
}

