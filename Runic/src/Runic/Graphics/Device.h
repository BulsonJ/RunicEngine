#pragma once
#include "Runic/Window.h"

// external
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <imgui.h>

#include <memory>

// internal
#include "Runic/Structures/DeletionQueue.h"
#include "Runic/Graphics/Internal/PipelineManager.h"
#include "Runic/Graphics/ResourceManager.h"


constexpr unsigned int FRAME_OVERLAP = 2U;

constexpr VkFormat DEFAULT_FORMAT = { VK_FORMAT_R8G8B8A8_SRGB };
constexpr VkFormat NORMAL_FORMAT = { VK_FORMAT_R8G8B8A8_UNORM };
constexpr VkFormat DEPTH_FORMAT = { VK_FORMAT_D32_SFLOAT };

namespace Runic
{
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

	struct RenderFrame
	{
		VkSemaphore presentSem;
		VkSemaphore	renderSem;
		VkFence renderFen;
	};

	class Device
	{
	public:
		void Init(Window* window);
		void Deinit();

		bool BeginFrame();
		void AddImGuiToCommandBuffer();
		void EndFrame();
		void Present();

		void WaitIdle();
		void WaitRender();

		/** Functions:
			- Resources
				-Buffers
				-Images
				-Samplers
				-Renderpass?
			- Swapchain/Rendering
				-Get backbuffer
				-Barriers
			- Set debug name
		
		*/

		void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

		// Move to private once device functions setup
		[[nodiscard]] int GetCurrentFrameNumber() { return m_frameNumber % FRAME_OVERLAP; }
		[[nodiscard]] RenderFrame& GetCurrentFrame() { return m_frame[GetCurrentFrameNumber()]; }

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
		bool m_dirtySwapchain {false};

		RenderFrame m_frame[FRAME_OVERLAP];
		int m_frameNumber{};

		VkRenderPass m_imguiPass;
		ImTextureID m_imguiRenderTexture;
		ImTextureID m_imguiDepthTexture;

		// Create render target function that caches these, rebuilds them on recreate swapchain?
		// Aren't stricly tied to device, just Renderer that is really deciding to create them as an extra implementation detail
		ImageHandle m_renderImage;
		ImageHandle m_depthImage;

		VkSampler m_defaultSampler;

		std::unique_ptr<PipelineManager> m_pipelineManager;

	private:
		void initVulkan();

		void initGraphicsCommands();
		void initComputeCommands();

		void createSwapchain();
		void recreateSwapchain();
		void destroySwapchain();
		void initSyncStructures();

		void initImguiRenderpass();
		void initImgui();
		void initImguiRenderImages();
	};
}
