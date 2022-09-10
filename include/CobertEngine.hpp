#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//3rd party
#include <vk_mem_alloc.h>
//

#include <iostream>
#include <vector>
#include <unordered_map>

#define GLM_ENABLE_EXPERIMENTAL 
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/hash.hpp>

#include <optional>
#include <set>
#include <limits>
#include <algorithm>
#include <fstream>
#include <vulkan/vulkan.hpp>
/*glm::vec3 pos;
		glm::vec3 normal;
		glm::vec3 colour;
		glm::vec2 uv;*/

namespace CobertEngine {

	struct QueueFamilyInfo {
		std::optional<uint32_t> graphics;
		std::optional<uint32_t> present;

		bool completed() {
			return (graphics.has_value() && present.has_value());
		}
	};

	struct SwapChainSupportDetails {
		vk::SurfaceCapabilitiesKHR capabilities;
		vk::SurfaceFormatKHR surfaceFormat;
		vk::PresentModeKHR presentMode;
		bool supported;
	};

	struct AllocatedBuffer {
		vk::Buffer buffer;
		VmaAllocation allocation;
	};

	struct AllocatedImage {
		vk::Image image;
		VmaAllocation allocation;
	};

	struct VertInputDescription {
		std::vector<vk::VertexInputBindingDescription> bindings;
		std::vector<vk::VertexInputAttributeDescription> attributes;

		vk::PipelineVertexInputStateCreateFlags flags;
	};

	struct MeshPushConstants {
		glm::vec4 data;
		glm::mat4 render_matrix;
	};

	struct GPUCameraData{
		glm::mat4 view;
		glm::mat4 proj;
		glm::mat4 viewproj;
	};

	struct FrameData{
		AllocatedBuffer camerabuffer;
		AllocatedBuffer objectbuffer;
		vk::DescriptorSet globalDescriptor;
		vk::DescriptorSet objectDescriptor;
	};

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec3 colour;
		glm::vec2 uv;

		static VertInputDescription get_vertex_description();

		bool operator==(const Vertex& other) const {
			return pos==other.pos && normal==other.normal && colour==other.colour && uv==other.uv;
		};
	};

	struct Material {
		vk::Pipeline pipeline;
		vk::PipelineLayout pipelinelayout;
		vk::DescriptorSet textureSet{VK_NULL_HANDLE};
	};

	struct Texture {
		AllocatedImage allocatedimage;
		vk::ImageView imageview;
	};

	struct CobertMesh {
		std::vector<Vertex> vertices;
		std::vector<uint32_t> vertexindices;

		AllocatedBuffer vertexbuffer;
		AllocatedBuffer indexbuffer;
		
		bool load_obj(std::string filename);
	};

	struct WorldObject {
		CobertMesh* mesh;
		Material* material;
		glm::mat4 transform;
	};

	struct UploadContext {
		vk::Fence uploadFence;
		vk::CommandPool uploadCommandPool;
		vk::CommandBuffer uploadCommandBuffer;
	};

	struct GPUObjectData {
		glm::mat4 objectMatrix;
	};

	struct GPUWorldData {
		glm::vec4 fogColor;
		glm::vec4 fogDistances;
		glm::vec4 ambientColor;
		glm::vec4 sunlightDirection;
		glm::vec4 sunlightColor;
	};

	class VulkanCore {

	private:
	public:
		VulkanCore(uint32_t WIDTH, uint32_t HEIGHT);
		void GetNewSwapchain();

		GLFWwindow* window;
		vk::Instance instance;
		vk::DebugUtilsMessengerEXT debugMessenger;
		vk::DispatchLoaderDynamic instanceLoader;

		VkSurfaceKHR csurface;
		vk::UniqueSurfaceKHR surface;

		vk::PhysicalDevice gpu;
		vk::Device gpudevice;
		vk::PhysicalDeviceProperties gpuproperties;
		size_t uniformbuffer_alignment = 4;

		vk::Queue graphicsQueue;
		vk::Queue presentQueue;

		uint32_t windowWidth;
		uint32_t windowHeight;

		QueueFamilyInfo queueFamilies;
		SwapChainSupportDetails swapchainDetails;

		
		void cleanup();

	};

	class Engine {

	private:	

		QueueFamilyInfo* deviceQueueFams;
		SwapChainSupportDetails* swapchainDetails;

		vk::Extent2D swapchainExtent;
		vk::SwapchainKHR swapchain;

		std::vector<vk::Image> swapchainImages;
		std::vector<vk::ImageView> swapchainImageViews;
		std::vector<vk::Framebuffer> swapChainFramebuffer;
		std::vector<FrameData> framedatas;

		vk::ImageView depthBufferImageView;
		AllocatedImage depthBufferImage;
		
		vk::RenderPass renderpass;
		//vk::PipelineLayout pipelineLayout;
		vk::Pipeline graphicsPipeline;
		vk::PipelineLayout meshPipelineLayout;

		vk::DescriptorSetLayout descriptorSetLayout;
		vk::DescriptorSetLayout objectSetLayout;
		vk::DescriptorSetLayout textureSetLayout;

		vk::DescriptorPool descriptorPool;

		vk::CommandPool cmdPool;
		std::vector<vk::CommandBuffer> cmdBuffer;

		std::vector<vk::Semaphore> imageAvailableSem;
		std::vector<vk::Semaphore> renderDoneSem;
		std::vector<vk::Fence> frameFlightFen;

		UploadContext uploadContext;

		int flightFrames = 2;
		int currentFrame = 0;
		float globaltick = 0.0f;

		bool cursorlocked = false;
		bool oldcursorlock = true;

		void AllocatorCreate();
		void SwapchainCreate();
		void SwapchainRecreate();
		void RenderPassCreate();
		void DescriptorsCreate();
		void MeshPipelineLayoutCreate();
		void GraphicsPipelineCreate();
		void FramebufferCreate();
		void CommandPoolCreate();
		void SyncObjectsCreate();

		void EngineStep();

		void load_scene();
		void load_meshes();
		void load_textures();
		AllocatedBuffer create_allocatedbuffer(size_t allocSize, vk::BufferUsageFlagBits usage, VmaMemoryUsage memoryUsage);
		void upload_mesh(CobertMesh& mesh);	

		std::unordered_map<int,bool> keyboard;
		double mousex;
		double mousey;

		double mousedeltax;
		double mousedeltay;

		double camerarotx;
		double cameraroty;

		glm::vec3 camPos = {0.f,0.f,-2.f};

		glm::mat4 camMat;

		std::vector<WorldObject> _world;
		GPUWorldData _worldParams;
		AllocatedBuffer _worldParamsBuffer;
		std::unordered_map<std::string,Material> _materials;
		std::unordered_map<std::string,CobertMesh> _meshes;
		std::unordered_map<std::string,Texture> _textures;

		std::vector<vk::Sampler> samplers;

		std::vector<Texture> loadedimages;

		Material* create_material(vk::Pipeline pipeline,vk::PipelineLayout layout,const std::string& name);
		Material* get_material(const std::string& name);
		void load_material_image(Material* target,std::string name);
		CobertMesh* get_mesh(const std::string& name);
		void draw_world(vk::CommandBuffer*& cmdBufferCurrent, WorldObject* first, int count, float globaltick);


	public:	

		bool windowCurrentlyResized = false;
		VmaAllocator vallocator;

		//vk::Pipeline meshPipeline;
		//CobertMesh mesh;
		//CobertMesh loadedmesh;
		void immediate_submit(std::function<void(vk::CommandBuffer cmd)>&& function);
		void key_input(int key, int scancode, int action, int mods);
		void mouse_input(double x, double y);
		
		Engine(int FF);	
		void cleanup();
		VulkanCore* core;
		
	};


	bool load_image_file(Engine engine, const char* file, AllocatedImage& out);


	void start();

}

namespace std {
	template<> 
	struct hash<CobertEngine::Vertex> {
		size_t operator()(CobertEngine::Vertex const& vertex) const {
			
			size_t h1 = hash<glm::vec3>()(vertex.pos);
			size_t h2 = hash<glm::vec3>()(vertex.normal);
			size_t h3 = hash<glm::vec3>()(vertex.colour);
			size_t h4 = hash<glm::vec2>()(vertex.uv);

			return ((h1 ^ (h2<<1)) >> 1) ^ (h3 ^ (h4<<1));

		}
	};
}