#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm.hpp>

#include <functional>
#include <imgui.h>
#include <unordered_map>

#include "Runic/Graphics/Internal/PipelineBuilder.h"
#include "Runic/Graphics/ResourceManager.h"
#include "Runic/Window.h"

#include "Runic/Structures/DeletionQueue.h"

#include "Runic/Graphics/Mesh.h"
#include "Runic/Graphics/Texture.h"
#include "Runic/Scene/Entity.h"
#include "Runic/Scene/Components/RenderableComponent.h"
#include "Runic/Scene/Camera.h"

constexpr unsigned int FRAME_OVERLAP = 2U;
constexpr unsigned int MAX_OBJECTS = 1024;
constexpr unsigned int MAX_TEXTURES = 128;
constexpr glm::vec3 UP_DIR = { 0.0f,1.0f,0.0f };
constexpr VkFormat DEFAULT_FORMAT = { VK_FORMAT_R8G8B8A8_SRGB };
constexpr VkFormat NORMAL_FORMAT = { VK_FORMAT_R8G8B8A8_UNORM };
constexpr VkFormat DEPTH_FORMAT = { VK_FORMAT_D32_SFLOAT };

struct SDL_Window;

namespace Runic
{
	struct WindowContext
	{
		SDL_Window* window = { nullptr };
		VkExtent2D extent = { 1920 , 1080 };
		bool resized{ false };
	};

	struct CommandContext
	{
		VkCommandPool pool;
		VkCommandBuffer buffer;
	};

	template<uint32_t FRAMES>
	struct QueueContext
	{
		VkQueue queue;
		uint32_t queueFamily;
		CommandContext commands[FRAMES];
	};

	struct UploadContext
	{
		VkFence uploadFence;
		VkCommandPool commandPool;
		VkCommandBuffer commandBuffer;
	};

	struct Swapchain
	{
		VkSwapchainKHR swapchain;
		VkFormat imageFormat;
		std::vector<VkImage> images;
		std::vector<VkImageView> imageViews;
		std::vector<VkFramebuffer> framebuffers;
	};

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
			glm::vec4 direction = { -0.15f, 0.1f, 0.4f, 1.0f };
			glm::vec4 color = { 1.0f,1.0f,1.0f,1.0f };
			glm::vec4 ambientColor = { 0.7f, 0.7f, 0.7f, 1.0f };
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

	struct RenderFrame
	{
		VkSemaphore presentSem;
		VkSemaphore	renderSem;
		VkFence renderFen;

		VkDescriptorSet globalSet;
		BufferHandle transformBuffer;
		BufferHandle materialBuffer;
		BufferHandle drawDataBuffer;

		VkDescriptorSet sceneSet;
		BufferHandle cameraBuffer;
		BufferHandle dirLightBuffer;
	};

	class Renderer
	{
	public:
		void init(Window* window);
		void deinit();

		// Public rendering API
		void draw(Camera* const camera);
		void GiveRenderables(const std::vector<std::shared_ptr<Runic::Entity>>& entities);
		MeshHandle uploadMesh(const MeshDesc& mesh);
		TextureHandle uploadTexture(const Texture& texture);
		void setSkybox(TextureHandle texture);

		bool m_dirtySwapchain{ false };
	private:
		void initVulkan();
		void initImguiRenderpass();
		void createSwapchain();
		void recreateSwapchain();
		void destroySwapchain();

		void initGraphicsCommands();
		void initComputeCommands();
		void initSyncStructures();

		void initImgui();
		void initImguiRenderImages();
		void initShaders();

		void initShaderData();

		void drawObjects(VkCommandBuffer cmd, const std::vector<Runic::Entity*>& renderObjects);

		ImageHandle uploadTextureInternal(const Runic::Texture& image);
		ImageHandle uploadTextureInternalCubemap(const Runic::Texture& image);

		void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

		[[nodiscard]] int getCurrentFrameNumber() { return m_frameNumber % FRAME_OVERLAP; }
		[[nodiscard]] RenderFrame& getCurrentFrame() { return m_frame[getCurrentFrameNumber()]; }

		Window* m_window;

		VkInstance m_instance;
		VkPhysicalDevice m_chosenGPU;
		VkPhysicalDeviceProperties m_gpuProperties;
		VkDevice m_device;
		DeletionQueue m_instanceDeletionQueue;

		VkSurfaceKHR m_surface;
		VmaAllocator m_allocator;
		VkDebugUtilsMessengerEXT m_debugMessenger;

		Runic::QueueContext<FRAME_OVERLAP> m_graphics;
		Runic::QueueContext<1> m_compute;
		Runic::UploadContext m_uploadContext;

		Runic::Swapchain m_swapchain;
		uint32_t m_currentSwapchainImage;

		VkRenderPass m_imguiPass;
		ImTextureID m_imguiRenderTexture;
		ImTextureID m_imguiDepthTexture;

		RenderFrame m_frame[FRAME_OVERLAP];
		ImageHandle m_renderImage;
		ImageHandle m_depthImage;
		int m_frameNumber{};

		std::unique_ptr<PipelineManager> m_pipelineManager;

		VkDescriptorSetLayout m_globalSetLayout;
		VkDescriptorPool m_globalPool;

		VkDescriptorSetLayout m_sceneSetLayout;
		VkDescriptorPool m_scenePool;

		Camera* m_currentCamera;
		GPUData::DirectionalLight m_sunlight;

		Slotmap<RenderMesh> m_meshes;
		std::unordered_map<std::string, MaterialType> m_materials;
		Slotmap<ImageHandle> m_bindlessImages;
		VkSampler m_defaultSampler;

		RenderableComponent m_skybox;
		MeshHandle m_skyboxMesh;
		TextureHandle m_skyboxTexture;

		std::vector<Runic::Entity*> m_entities;
	};
}