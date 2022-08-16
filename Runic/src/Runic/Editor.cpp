#include "Runic/Editor.h"

#include <imgui_internal.h.>
#include <backends/imgui_impl_vulkan.h>

#include "Runic/Log.h"
#include <memory>

using namespace Runic;

static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

namespace Runic
{
	namespace Editor
	{
		ImTextureID ViewportTexture;
		ImTextureID ViewportDepthTexture;

		glm::vec4* lightDirection;
		glm::vec4* lightColor;
		glm::vec4* lightAmbientColor;
	}
}

void Editor::DrawEditor()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Editor", nullptr, window_flags);
	ImGui::PopStyleVar();
	ImGui::PopStyleVar(2);

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("Editor");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

		static auto first_time = true;
		if (first_time)
		{
			first_time = false;

			ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
			ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

			// split the dockspace into 2 nodes -- DockBuilderSplitNode takes in the following args in the following order
			//   window ID to split, direction, fraction (between 0 and 1), the final two setting let's us choose which id we want (which ever one we DON'T set as NULL, will be returned by the function)
			//                                                              out_id_at_dir is the id of the node in the direction we specified earlier, out_id_at_opposite_dir is in the opposite direction
			auto dock_id_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.2f, nullptr, &dockspace_id);

			// we now dock our windows into the docking node we made above
			ImGui::DockBuilderDockWindow("Viewport", dockspace_id);
			ImGui::DockBuilderDockWindow("Log", dock_id_right);
			ImGui::DockBuilderDockWindow("SceneGraph", dock_id_right);
			ImGui::DockBuilderDockWindow("Viewport Depth", dockspace_id);
			ImGui::DockBuilderFinish(dockspace_id);
		}
	}

	ImGui::End();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	DrawViewport();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar(2);
	DrawViewportDepth();
	DrawSceneGraph();
	DrawLog();
}

void Editor::DrawViewport()
{
	ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	//ImVec2 regionSize = ImGui::GetContentRegionMax();
	//float aspectRatio = 16.0f / 9.0f;
	//
	//float new_height = (9.0f * regionSize.x) / 16.0f;
	ImGui::Image(Editor::ViewportTexture, ImGui::GetContentRegionMax());
	ImGui::End();
}

void Editor::DrawViewportDepth() {
	ImGui::Begin("Viewport Depth", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing);
	ImGui::Image(Editor::ViewportDepthTexture, ImGui::GetContentRegionMax());
	ImGui::End();
}

void Editor::DrawSceneGraph()
{
	ImGui::Begin("SceneGraph");
	ImGui::DragFloat3("Light Direction", (float*)lightDirection, 0.05f, -1.0f, 1.0f);
	ImGui::ColorEdit4("Light Color", (float*)lightColor, ImGuiColorEditFlags_DisplayRGB);
	ImGui::ColorEdit4("Light Ambient Color", (float*)lightAmbientColor, ImGuiColorEditFlags_DisplayRGB);
	ImGui::End();
}

void Editor::DrawLog()
{
	ImGui::Begin("Log");

	const auto newLogStream = Log::GetCoreLoggerStream().str();
	static auto currentlogStream = Log::GetCoreLoggerStream().str();
	ImGui::TextWrapped(currentlogStream.c_str());
	if (currentlogStream != newLogStream)
	{
		ImGui::SetScrollHereY(1.0f);
		currentlogStream = newLogStream;
	}

	ImGui::End();
}
