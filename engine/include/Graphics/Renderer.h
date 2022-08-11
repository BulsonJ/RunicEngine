#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm.hpp>

#include <functional>
#include <imgui.h>
#include <unordered_map>

#include "PipelineBuilder.h"
#include "ResourceManager.h"
#include "Mesh.h"
#include "DeletionQueue.h"
#include "RenderableTypes.h"

constexpr unsigned int FRAME_OVERLAP = 2U;
constexpr unsigned int MAX_OBJECTS = 100;
constexpr glm::vec3 UP_DIR = { 0.0f,1.0f,0.0f };
constexpr VkFormat DEFAULT_FORMAT = { VK_FORMAT_R8G8B8A8_SRGB };
constexpr VkFormat NORMAL_FORMAT = { VK_FORMAT_R8G8B8A8_UNORM };

struct SDL_Window;

namespace RenderTypes
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
}

namespace GPUShaderData
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

struct RenderMesh
{
	RenderableTypes::MeshDesc meshDesc;
	BufferHandle vertexBuffer;
	BufferHandle indexBuffer;

	static VertexInputDescription getVertexDescription();
};

struct MaterialType
{
	VkPipeline pipeline = { VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout = { VK_NULL_HANDLE };
};

struct MaterialInstance
{
	MaterialType* matType = nullptr;
	GPUShaderData::Material materialData;
};

struct RenderFrame
{
	ImageHandle renderImage;

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
	void init();
	void deinit();

	// Public rendering API
	void draw(const std::vector<RenderableTypes::RenderObject>& renderObjects);
	RenderableTypes::MeshHandle uploadMesh(const RenderableTypes::MeshDesc& mesh);
	RenderableTypes::TextureHandle uploadTexture(const RenderableTypes::Texture& texture);

	RenderTypes::WindowContext window;
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

	void drawObjects(VkCommandBuffer cmd, const std::vector<RenderableTypes::RenderObject>& renderObjects);

	ImageHandle uploadTextureInternal(const RenderableTypes::Texture& image);

	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

	[[nodiscard]] int getCurrentFrameNumber() { return frameNumber % FRAME_OVERLAP; }
	[[nodiscard]] RenderFrame& getCurrentFrame() { return frame[getCurrentFrameNumber()]; }

	VkInstance instance;
	VkPhysicalDevice chosenGPU;
	VkPhysicalDeviceProperties gpuProperties;
	VkDevice device;
	DeletionQueue instanceDeletionQueue;

	VkSurfaceKHR surface;
	VmaAllocator allocator;
	VkDebugUtilsMessengerEXT debugMessenger;

	RenderTypes::QueueContext<FRAME_OVERLAP> graphics;
	RenderTypes::QueueContext<1> compute;
	RenderTypes::UploadContext uploadContext;

	RenderTypes::Swapchain swapchain;
	uint32_t currentSwapchainImage;

	VkRenderPass imguiPass;
	ImTextureID imguiRenderTexture[FRAME_OVERLAP];
	ImTextureID imguiDepthTexture;

	RenderFrame frame[FRAME_OVERLAP];
	ImageHandle depthImage;
	int frameNumber{};

	VkDescriptorSetLayout globalSetLayout;
	VkDescriptorPool globalPool;

	VkDescriptorSetLayout sceneSetLayout;
	VkDescriptorPool scenePool;

	GPUShaderData::Camera camera;
	GPUShaderData::DirectionalLight sunlight;

	Slotmap<RenderMesh> meshes;
	std::unordered_map<std::string, MaterialType> materials;

	Slotmap<ImageHandle> bindlessImages;
};