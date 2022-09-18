#pragma once

#include <glm.hpp>

#include <functional>
#include <imgui.h>
#include <unordered_map>

#include "Runic/Graphics/Internal/PipelineBuilder.h"
#include "Runic/Graphics/ResourceManager.h"

#include "Runic/Graphics/Device.h"
#include "Runic/Graphics/Mesh.h"
#include "Runic/Graphics/Texture.h"
#include "Runic/Scene/Entity.h"
#include "Runic/Scene/Components/RenderableComponent.h"
#include "Runic/Scene/Components/LightComponent.h"
#include "Runic/Scene/Camera.h"

constexpr unsigned int MAX_OBJECTS = 1024;
constexpr unsigned int MAX_TEXTURES = 128;
constexpr unsigned int MAX_POINT_LIGHTS = 4U;
constexpr glm::vec3 UP_DIR = { 0.0f,1.0f,0.0f };

struct SDL_Window;

namespace Runic
{

	namespace GPUData
	{

		struct PushConstants
		{
			int drawDataIndex;
		};

		struct DrawData
		{
			int transformIndex;
			int materialIndex;
			int padding[2];
		};

		struct Material
		{
			glm::vec4 diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
			glm::vec3 specular = { 1.0f, 1.0f, 1.0f };
			float shininess = { 32.0f };
			glm::ivec4 textureIndices;
		};

		struct Transform
		{
			glm::mat4 modelMatrix{};
			glm::mat4 normalMatrix{};
		};

		struct DirectionalLight
		{
			glm::vec4 ambient = { 0.0f, 0.0f, 0.0f, 0.0f };
			glm::vec4 diffuse = { 0.0f,0.0f,0.0f,0.0f };
			glm::vec4 specular = { 0.0f, 0.0f, 0.0f, 1.0f };
			glm::vec4 direction = { 0.0f, 0.0f, 0.0f, 1.0f };
		};

		struct PointLight
		{
			glm::vec4 ambient = { 0.7f, 0.7f, 0.7f, 1.0f };
			glm::vec4 diffuse = { 1.0f,1.0f,1.0f,1.0f };
			glm::vec4 specular = { 0.7f, 0.7f, 0.7f, 1.0f };
			glm::vec4 position = { 0.0f,0.0f,0.0f, 1.0f };

			float constant;
			float linear;
			float quadratic;
			float padding;
		};

		struct Camera
		{
			glm::mat4 view{};
			glm::mat4 proj{};
			glm::vec4 pos{};
		};
	}

	struct VertexInputDescription
	{
		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attributes;
		VkPipelineVertexInputStateCreateFlags flags = 0;
	};

	struct PipelineInfo
	{
		std::string name;
		VkPipeline pipeline{};
		VkPipelineLayout pipelineLayout{};

		std::string vertexShader;
		std::string fragmentShader;

		bool enableDepthWrite {true};
	};

	typedef uint32_t PipelineHandle;

	class PipelineManager
	{
	public:
		PipelineManager(VkDevice device) : m_device(device){}
		void Deinit();
		VkPipeline GetPipeline(PipelineHandle handle);
		VkPipelineLayout CreatePipelineLayout(std::vector<VkDescriptorSetLayout> descLayouts,
											  std::vector<VkPushConstantRange> pushConstants);
		PipelineHandle CreatePipeline(PipelineInfo info);
		void RecompilePipelines();
	private:
		VkPipeline createPipelineInternal(PipelineInfo info);
		VkDevice m_device;
		std::string m_sourceFolder;
		std::vector<VkPipelineLayout> m_pipelineLayouts;
		std::vector<PipelineInfo> m_pipelines;
	};

	struct RenderMesh
	{
		Runic::MeshDesc meshDesc;
		BufferHandle vertexBuffer;
		BufferHandle indexBuffer;

		static VertexInputDescription getVertexDescription();
	};

	struct MaterialType
	{
		PipelineHandle pipeline = { 0};
		VkPipelineLayout pipelineLayout = { VK_NULL_HANDLE };
	};

	struct MaterialInstance
	{
		MaterialType* matType = nullptr;
		GPUData::Material materialData;
	};

	struct RenderFrameObjects
	{
		VkDescriptorSet globalSet;
		BufferHandle transformBuffer;
		BufferHandle materialBuffer;
		BufferHandle drawDataBuffer;

		VkDescriptorSet sceneSet;
		BufferHandle cameraBuffer;
		BufferHandle dirLightBuffer;
		BufferHandle pointLightBuffer;
	};

	class Renderer
	{
	public:
		void init(Device* device);
		void deinit();

		// Public rendering API
		void draw(Camera* const camera);
		void GiveRenderables(const std::vector<std::shared_ptr<Runic::Entity>>& entities);
		MeshHandle uploadMesh(const MeshDesc& mesh);
		TextureHandle uploadTexture(const Texture& texture);
		void setSkybox(TextureHandle texture);
	private:
		void initShaders();

		void initShaderData();

		void drawObjects(VkCommandBuffer cmd, const std::vector<Runic::Entity*>& renderObjects);

		ImageHandle uploadTextureInternal(const Runic::Texture& image);
		ImageHandle uploadTextureInternalCubemap(const Runic::Texture& image);

		[[nodiscard]] RenderFrameObjects& getCurrentFrame() { return m_frame[m_graphicsDevice->getCurrentFrameNumber()]; }

		Device* m_graphicsDevice;
		RenderFrameObjects m_frame[FRAME_OVERLAP];

		std::unique_ptr<PipelineManager> m_pipelineManager;

		VkDescriptorSetLayout m_globalSetLayout;
		VkDescriptorPool m_globalPool;

		VkDescriptorSetLayout m_sceneSetLayout;
		VkDescriptorPool m_scenePool;

		Camera* m_currentCamera;

		Slotmap<RenderMesh> m_meshes;
		std::unordered_map<std::string, MaterialType> m_materials;
		Slotmap<ImageHandle> m_bindlessImages;

		RenderableComponent m_skybox;
		MeshHandle m_skyboxMesh;
		TextureHandle m_skyboxTexture;

		std::vector<Runic::Entity*> m_entities;
		std::map<LightComponent::LightType, std::vector<Runic::Entity*>> m_entities_with_lights;
	};
}