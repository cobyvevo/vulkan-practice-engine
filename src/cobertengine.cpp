#define VMA_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION 
#define STB_IMAGE_IMPLEMENTATION

#include <vk_mem_alloc.h>
#include <tiny_obj_loader.h>
#include <stb_image.h>

#include "CobertEngine.hpp"

const std::vector<const char*> valLayers = {
    "VK_LAYER_KHRONOS_validation"
};
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

std::vector<const char*> getGLFWExtensions(bool enableValLayers) {
	uint32_t glfwExtensionCount = 0;

	const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> allextensions(glfwExts,glfwExts+glfwExtensionCount);

	if (enableValLayers == true) {
		allextensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	for (const char* extension: allextensions) {
		std::cout << extension << std::endl;
	}

	return allextensions;
}

vk::ShaderModule setupShader(const std::vector<char>& shader, vk::Device& gpudevice) {
	vk::ShaderModuleCreateInfo createinfo(
		{},
		shader.size(),
		reinterpret_cast<const uint32_t*>(shader.data())
	);

	vk::ShaderModule module = gpudevice.createShaderModule(createinfo);

	return module;
}

size_t pad_uniform_buffer_size(size_t original, size_t minUboAlignment) {
	if (minUboAlignment > 0) {
		return (original + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return original;
}

void vkdo(VkResult x, const char* error) {
	if (x != VK_SUCCESS) {throw std::runtime_error(error);}
}

bool checkDeviceSupport(vk::PhysicalDevice* device) {
	std::vector<vk::ExtensionProperties> device_extensions = device->enumerateDeviceExtensionProperties();
	std::set<std::string> required_extensions(deviceExtensions.begin(),deviceExtensions.end());
	
	for (const vk::ExtensionProperties& extension: device_extensions) {
		required_extensions.erase(extension.extensionName);
		std::cout << "got extension support for" << extension.extensionName << std::endl;
	}

	if (required_extensions.empty()) std::cout << "successful" << std::endl;

	return required_extensions.empty();
}

namespace CobertEngine {

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	        VkDebugUtilsMessageTypeFlagsEXT messageType,
	        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	        void* pUserData) {
	    

	    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
	    
	    return VK_FALSE;
	}

	static std::vector<char> readFile(const std::string& filename) {
	    std::ifstream file(filename, std::ios::ate | std::ios::binary);

	    if (!file.is_open()) {
	        throw std::runtime_error("file didnt open you mongoloid");
	    }

	    size_t fileSize = (size_t) file.tellg();
	    std::vector<char> buffer(fileSize);

	    file.seekg(0);
	    file.read(buffer.data(),fileSize);

	    file.close();

	    return buffer;
	}

	AllocatedBuffer Engine::create_allocatedbuffer(size_t allocSize, vk::BufferUsageFlagBits usage, VmaMemoryUsage memoryUsage) {
		vk::BufferCreateInfo bufferinfo{};

		bufferinfo.size = allocSize; //in bytes
		bufferinfo.usage = usage;

		VmaAllocationCreateInfo vmaallocinfo{};
		vmaallocinfo.usage = memoryUsage;

		AllocatedBuffer newbuffer;

		if (vmaCreateBuffer(
			vallocator,
			reinterpret_cast<VkBufferCreateInfo*>(&bufferinfo),
			&vmaallocinfo,
			reinterpret_cast<VkBuffer*>(&newbuffer.buffer),
			&newbuffer.allocation,
			nullptr
			) != VK_SUCCESS) {
			throw std::runtime_error("mesh upload allocation failed");
		}

		return newbuffer;
	}

	SwapChainSupportDetails getSwapchainDetails(vk::PhysicalDevice* device, vk::UniqueSurfaceKHR* uniquesurface) {
		SwapChainSupportDetails details;
		auto surface = uniquesurface->get();

		details.capabilities = device->getSurfaceCapabilitiesKHR(surface);
		std::vector<vk::SurfaceFormatKHR> formats = device->getSurfaceFormatsKHR(surface);
		std::vector<vk::PresentModeKHR> presentmodes = device->getSurfacePresentModesKHR(surface);
			
		if (formats.empty() || presentmodes.empty()) {
			details.supported = false;
			return details;
		}
		details.supported = true;

		for (const vk::SurfaceFormatKHR& available: formats) {
			if (available.format == vk::Format::eB8G8R8A8Srgb && available.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
				details.surfaceFormat = available;
				break;
			}
		}
		
		for (const vk::PresentModeKHR& available: presentmodes) {
			if (available == vk::PresentModeKHR::eMailbox) {
				details.presentMode = available;
				break;
			}
		}

		return details;
	}

	QueueFamilyInfo getQueueFamsFromDevice(vk::PhysicalDevice* device, vk::UniqueSurfaceKHR* surface) {
		QueueFamilyInfo indices;
		//get list of queue families
		//pick out graphics family and present family
		//uint32_t qfc = 0; 
		std::vector<vk::QueueFamilyProperties> queueFams = device->getQueueFamilyProperties();

		int i = 0;
		for (const auto& family : queueFams) {

			std::cout << vk::to_string(family.queueFlags) << std::endl;
			if (family.queueFlags & vk::QueueFlagBits::eGraphics) {
				std::cout << "found graphics" << std::endl;
				indices.graphics = i;
			} 

			if (device->getSurfaceSupportKHR(i, surface->get())) {
				std::cout << "found present > " << i << std::endl;
				indices.present = i;
			}

			if (indices.completed()) break;
			i++;

		}

		return indices;
	}
	//vulkan
	VulkanCore::VulkanCore(uint32_t WIDTH, uint32_t HEIGHT) {

		std::cout << "creating boilerplate" << std::endl;
		//window stuff
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window = glfwCreateWindow(WIDTH,HEIGHT, "epic gaming", nullptr, nullptr);

		windowWidth = WIDTH;
		windowHeight = HEIGHT;
		//

		bool validationLayersEnabled = true;

		std::vector<const char*> glfwExt = getGLFWExtensions(validationLayersEnabled);

		vk::ApplicationInfo appinfo{};
		appinfo.pApplicationName = "App";
		appinfo.applicationVersion = 1;
		appinfo.pEngineName = "cobert engine v1";
		appinfo.engineVersion = 1;
		appinfo.apiVersion = VK_API_VERSION_1_3;

		vk::InstanceCreateInfo createinfo{};
		createinfo.pApplicationInfo = &appinfo;
		createinfo.enabledLayerCount = valLayers.size();
		createinfo.ppEnabledLayerNames = valLayers.data();
		createinfo.enabledExtensionCount = glfwExt.size();
		createinfo.ppEnabledExtensionNames = glfwExt.data();

		instance = vk::createInstance(createinfo);

		//making surface
		vkdo(
			glfwCreateWindowSurface(instance, window, nullptr, &csurface),
			"The window surface failed"
		);

		surface = vk::UniqueSurfaceKHR(csurface, instance);

		//creating debug messenger
		vk::DebugUtilsMessageSeverityFlagsEXT severities(
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | 
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | 
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
		);

		vk::DebugUtilsMessageTypeFlagsEXT types(
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | 
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | 
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
		);

		vk::DebugUtilsMessengerCreateInfoEXT debugcreateinfo{};
		debugcreateinfo.messageSeverity = severities;
		debugcreateinfo.messageType = types;
		debugcreateinfo.pfnUserCallback = &debugCallback;

		instanceLoader = vk::DispatchLoaderDynamic(instance, vkGetInstanceProcAddr);

		debugMessenger = instance.createDebugUtilsMessengerEXT(debugcreateinfo,nullptr,instanceLoader);

		//get device
		std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
		for (auto& device : devices) {
			//get queue family information
			std::cout << device.getProperties().deviceName << std::endl;
			
			QueueFamilyInfo indices = getQueueFamsFromDevice(&device, &surface);
			bool supportAdequate = checkDeviceSupport(&device);
			SwapChainSupportDetails swapdetails = getSwapchainDetails(&device, &surface);

			if (indices.completed() && supportAdequate && swapdetails.supported) {
			
				std::cout << "chosen" << std::endl;

				queueFamilies = indices;
				swapchainDetails = swapdetails;
				gpu = device;
				gpuproperties = device.getProperties();
				uniformbuffer_alignment = gpuproperties.limits.minUniformBufferOffsetAlignment;

			}

		}

		std::cout << "this GPU allows minimum buffer alighment of" << gpuproperties.limits.minUniformBufferOffsetAlignment << std::endl;

		//create logical device
		std::vector<vk::DeviceQueueCreateInfo> queueCreateinfos;
		std::set<uint32_t> queueFamilySet = {queueFamilies.graphics.value(),queueFamilies.present.value()};

		float priority = 1.0f;
		for (uint32_t family : queueFamilySet) {
			vk::DeviceQueueCreateInfo queuecreateinfo{};
			queuecreateinfo.queueFamilyIndex = family;
			queuecreateinfo.queueCount = 1;
			queuecreateinfo.pQueuePriorities = &priority;

			queueCreateinfos.push_back(queuecreateinfo);
		}

		std::vector<const char*> desiredValidationLayers;
		if (validationLayersEnabled) desiredValidationLayers = valLayers;

		vk::PhysicalDeviceShaderDrawParametersFeatures features{};
		features.shaderDrawParameters = true;//TODO

		vk::DeviceCreateInfo deviceCreateinfo{};
		deviceCreateinfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateinfos.size());
		deviceCreateinfo.pQueueCreateInfos = queueCreateinfos.data();

		deviceCreateinfo.enabledLayerCount = static_cast<uint32_t>(desiredValidationLayers.size());
		deviceCreateinfo.ppEnabledLayerNames = desiredValidationLayers.data();

		deviceCreateinfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		deviceCreateinfo.ppEnabledExtensionNames = deviceExtensions.data();

		vk::StructureChain<vk::DeviceCreateInfo,vk::PhysicalDeviceShaderDrawParametersFeatures> chain = {deviceCreateinfo,features};

		//deviceCreateinfo.pEnabledFeatuers = features;

		gpudevice = gpu.createDevice(chain.get<vk::DeviceCreateInfo>()); //fuck chain
		graphicsQueue = gpudevice.getQueue(queueFamilies.graphics.value(),0);
		presentQueue = gpudevice.getQueue(queueFamilies.present.value(),0);
		std::cout << "created logical device" << std::endl;

	}

	void VulkanCore::GetNewSwapchain() {
		swapchainDetails = getSwapchainDetails(&gpu, &surface);
	}

	void VulkanCore::cleanup() {

		gpudevice.waitIdle();

		std::cout << "cleaning up vulkan boilerplate" << std::endl;

		instance.destroySurfaceKHR(csurface,nullptr);
		instance.destroyDebugUtilsMessengerEXT(debugMessenger,nullptr,instanceLoader);
		
		gpudevice.destroy();
		instance.destroy();
		glfwDestroyWindow(window);
		glfwTerminate();

		std::cout << "cleaning up successful" << std::endl;

	}
	//

	static void resizeCallback(GLFWwindow* win, int width, int height) { 
		auto me = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(win));
		me->windowCurrentlyResized = true;
	}
	//actual engine

	static void keycallback(GLFWwindow* win, int key, int scancode, int action, int mods){
		auto me = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(win));
		me->key_input(key, scancode, action, mods);
	}

	static void mousecallback(GLFWwindow* win, double x, double y){
		auto me = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(win));
		me->mouse_input(x,y);
	}

	//vertex

	VertInputDescription Vertex::get_vertex_description() {
		VertInputDescription desc;

		vk::VertexInputBindingDescription mainBinding{};
		mainBinding.binding = 0;
		mainBinding.stride = sizeof(Vertex);
		mainBinding.inputRate = vk::VertexInputRate::eVertex;

		desc.bindings.push_back(mainBinding);

		vk::VertexInputAttributeDescription posAtt{};
		posAtt.binding = 0;
		posAtt.location = 0;
		posAtt.format = vk::Format::eR32G32B32Sfloat;
		posAtt.offset = offsetof(Vertex, pos);

		vk::VertexInputAttributeDescription normAtt{};
		normAtt.binding = 0;
		normAtt.location = 1;
		normAtt.format = vk::Format::eR32G32B32Sfloat;
		normAtt.offset = offsetof(Vertex, normal);
		
		vk::VertexInputAttributeDescription colAtt{};
		colAtt.binding = 0;
		colAtt.location = 2;
		colAtt.format = vk::Format::eR32G32B32Sfloat;
		colAtt.offset = offsetof(Vertex, colour);

		vk::VertexInputAttributeDescription uvAtt{};
		uvAtt.binding = 0;
		uvAtt.location = 3;
		uvAtt.format = vk::Format::eR32G32Sfloat;
		uvAtt.offset = offsetof(Vertex, uv);

		desc.attributes.push_back(posAtt);
		desc.attributes.push_back(normAtt);
		desc.attributes.push_back(colAtt);
		desc.attributes.push_back(uvAtt);

		//Binding > ([PositionAtt - NormalAtt - ColorAtt],[PositionAtt - NormalAtt - ColorAtt],...)
		return desc;
	}

	//vertex

	//mesh

	bool CobertMesh::load_obj(std::string filename) {

		//tinyobj::attrib_t attrib; //vertex arrays
		//std::vector<tinyobj::shape_t> shapes; //info for each seperate object
		//std::vector<tinyobj::material_t> materials; //material data for each shape

		//std::string error;

		tinyobj::ObjReaderConfig reader_config;
		reader_config.mtl_search_path = "assets/";

		tinyobj::ObjReader reader;

		if (!reader.ParseFromFile(filename, reader_config)) {
			if (!reader.Error().empty()) {
				std::cerr << "LOAD_OBJ - ERROR:" << reader.Error() << std::endl;
				return false;
			}
		}

		if (!reader.Warning().empty()) {
			std::cerr << "LOAD_OBJ - WARNING:" << reader.Warning() << std::endl;
		}

		auto& attrib = reader.GetAttrib();
		auto& shapes = reader.GetShapes();
		//auto& materials = reader.GetMaterials();
	
		size_t index_offset = 0;

		std::unordered_map<Vertex, uint32_t> uniqueverts{};

		for (size_t s = 0; s < shapes.size(); s++) {
			
			//for (size_t ind = 0; ind < shapes[s].mesh.indices.size(); ind++) {

				//tinyobj::index_t idx = shapes[s].mesh.indices[ind];

			for (tinyobj::index_t idx : shapes[s].mesh.indices) {

				//vertex position
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				
				//vertex normal
				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

				//uv
				tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
				tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

				Vertex newvert;
				newvert.pos.x = vx;
				newvert.pos.y = vy;
				newvert.pos.z = vz;

				newvert.normal.x = nx;
				newvert.normal.y = ny;
				newvert.normal.z = nz;

				newvert.uv.x = ux;
				newvert.uv.y = 1-uy;

				newvert.colour = newvert.normal;

				if (uniqueverts.count(newvert) == 0) {
					uniqueverts[newvert] = (uint32_t) uniqueverts.size();
					vertices.push_back(newvert);
				} 
				std::cout << "index: " << uniqueverts[newvert] << std::endl;
				vertexindices.push_back(uniqueverts[newvert]);	

			}
		}
			
		return true;
	}

	//mesh

	//inputs 
	void Engine::key_input(int key, int scancode, int action, int mods) {
		if (action == GLFW_PRESS || action == GLFW_REPEAT) {

			if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
				if (cursorlocked == true) {
					cursorlocked = false;
				} else {
					cursorlocked = true;
				}
			};	

			keyboard[key] = true;
		} else {
			keyboard[key] = false;
		}

	}

	void Engine::mouse_input(double x, double y) {
		mousedeltax = x-mousex;
		mousedeltay = y-mousey;

		mousex = x;
		mousey = y;	
	}

	//
	Engine::Engine(int FF) {
		flightFrames = FF;

		core = new VulkanCore((uint32_t) 500,(uint32_t) 600);
			
		glfwSetWindowUserPointer(core->window,this);
		glfwSetFramebufferSizeCallback(core->window,resizeCallback);
		glfwSetKeyCallback(core->window,keycallback);
		glfwSetCursorPosCallback(core->window,mousecallback);

		deviceQueueFams = &core->queueFamilies;

		AllocatorCreate();
		
		SwapchainCreate();//swpachian details is done in here to make my life easier when recreating
		RenderPassCreate();
		DescriptorsCreate();
		MeshPipelineLayoutCreate();
		GraphicsPipelineCreate();
		FramebufferCreate();
		CommandPoolCreate();
		SyncObjectsCreate();

		load_meshes();
		load_textures();
		load_scene();

		std::cout << "engine created" << std::endl;

		while (!glfwWindowShouldClose(core->window)) {
			glfwPollEvents();
			EngineStep();
		}

		cleanup();

		delete core;
	}

	void Engine::load_scene() {

		camMat = glm::mat4{1.0f};
		create_material(graphicsPipeline, meshPipelineLayout, "defaultmesh");
		create_material(graphicsPipeline, meshPipelineLayout, "lostempire");

		Material* t = get_material("lostempire");

		load_material_image(t,"lost_empire_diffuse");
	
		WorldObject monkey;
		monkey.mesh = get_mesh("monkey");
		monkey.material = t;
		monkey.transform = glm::translate(glm::vec3{ 5,-10,0 });

		_world.push_back(monkey);

		for (int x = -20; x <= 20; x++) {
			for (int y = -20; y <= 20; y++) {

				WorldObject tri;
				tri.mesh = get_mesh("triangle");
				tri.material = get_material("defaultmesh");
				glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x, 0, y));
				glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
				tri.transform = translation * scale;

				_world.push_back(tri);

			}
		}

	};

	void Engine::load_meshes(){

		CobertMesh mesh;
		CobertMesh loadedmesh;

		mesh.vertices.resize(3);
		mesh.vertexindices.resize(3);

		mesh.vertices[0].pos = {0.8f,0.8f,0.0f};
		mesh.vertices[1].pos = {-0.8f,0.8f,0.0f};
		mesh.vertices[2].pos = {0.0f,-0.8f,0.0f};
		
		mesh.vertices[0].colour = {1.0f,0.0f,0.0f};
		mesh.vertices[1].colour = {0.0f,1.0f,0.0f};
		mesh.vertices[2].colour = {0.0f,0.0f,1.0f};

		mesh.vertexindices[0] = 0;
		mesh.vertexindices[1] = 1;
		mesh.vertexindices[2] = 2;
		
		loadedmesh.load_obj("assets/lost_empire.obj");

		upload_mesh(mesh);
		upload_mesh(loadedmesh);

		_meshes["monkey"] = loadedmesh;
		_meshes["triangle"] = mesh;

	} 

	void Engine::load_textures() {

		Texture lostEmpire;

		load_image_file(*this,"assets/lost_empire-RGBA.png", lostEmpire.allocatedimage);

		vk::ImageViewCreateInfo iview{};
		iview.image = lostEmpire.allocatedimage.image;
		iview.viewType = vk::ImageViewType::e2D;
		iview.format = vk::Format::eR8G8B8A8Srgb; //FORMAT
		iview.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		iview.subresourceRange.baseMipLevel = 0;
		iview.subresourceRange.levelCount = 1;
		iview.subresourceRange.baseArrayLayer = 0;
		iview.subresourceRange.layerCount = 1;

		lostEmpire.imageview = core->gpudevice.createImageView(iview);

		_textures["lost_empire_diffuse"] = lostEmpire;
		loadedimages.push_back(lostEmpire);


	}

	void Engine::immediate_submit(std::function<void(vk::CommandBuffer cmd)>&& function) {
		vk::CommandBuffer cmdbuffer = uploadContext.uploadCommandBuffer;

		vk::CommandBufferBeginInfo begininfo{};
		begininfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

		cmdbuffer.begin(begininfo);
		function(cmdbuffer);
		cmdbuffer.end();

		vk::SubmitInfo submitinfo{};
		submitinfo.commandBufferCount = 1;
		submitinfo.pCommandBuffers = &cmdbuffer;

		[[maybe_unused]] vk::Result res;
		res = core->graphicsQueue.submit(1, &submitinfo, uploadContext.uploadFence);
		res = core->gpudevice.waitForFences(1,&uploadContext.uploadFence, true, UINT64_MAX);
		res = core->gpudevice.resetFences(1, &uploadContext.uploadFence);
		//uploadContext.uploadCommandPool.reset();
		core->gpudevice.resetCommandPool(uploadContext.uploadCommandPool);

	};

	void Engine::upload_mesh(CobertMesh& mesh){
		/*
		vk::BufferCreateInfo bufferinfo{};

		bufferinfo.size = mesh.vertices.size() * sizeof(Vertex); //in bytes
		bufferinfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;

		VmaAllocationCreateInfo vmaallocinfo{};
		vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		if (vmaCreateBuffer(
			vallocator,
			reinterpret_cast<VkBufferCreateInfo*>(&bufferinfo),
			&vmaallocinfo,
			reinterpret_cast<VkBuffer*>(&mesh.vertexbuffer.buffer),
			&mesh.vertexbuffer.allocation,
			nullptr
			) != VK_SUCCESS) {
			throw std::runtime_error("mesh upload allocation failed");
		}

		//copy data

		void* data;
		vmaMapMemory(vallocator, mesh.vertexbuffer.allocation, &data);
		memcpy(data, mesh.vertices.data(), mesh.vertices.size()* sizeof(Vertex));

		vmaUnmapMemory(vallocator, mesh.vertexbuffer.allocation);*/

		const size_t bufferSize = mesh.vertices.size() * sizeof(Vertex);
		
		vk::BufferCreateInfo bufferinfo{};
		bufferinfo.size = bufferSize; //in bytes
		bufferinfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

		VmaAllocationCreateInfo vmaallocinfo{};
		//vmaallocinfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		//vmaallocinfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		AllocatedBuffer stagingBuffer;

		if (vmaCreateBuffer(
			vallocator,
			reinterpret_cast<VkBufferCreateInfo*>(&bufferinfo),
			&vmaallocinfo,
			reinterpret_cast<VkBuffer*>(&stagingBuffer.buffer),
			&stagingBuffer.allocation,
			nullptr
			) != VK_SUCCESS) {
			throw std::runtime_error("stagingbuffer allocation failed");
		}

		void* data;
		vmaMapMemory(vallocator, stagingBuffer.allocation, &data);
		memcpy(data, mesh.vertices.data(), mesh.vertices.size()* sizeof(Vertex));
		vmaUnmapMemory(vallocator, stagingBuffer.allocation);

		vk::BufferCreateInfo vertbufferinfo{};
		vertbufferinfo.size = bufferSize; //in bytes
		vertbufferinfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;

		//vmaallocinfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		//vmaallocinfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		vmaallocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		if (vmaCreateBuffer(
			vallocator,
			reinterpret_cast<VkBufferCreateInfo*>(&vertbufferinfo),
			&vmaallocinfo,
			reinterpret_cast<VkBuffer*>(&mesh.vertexbuffer.buffer),
			&mesh.vertexbuffer.allocation,
			nullptr
			) != VK_SUCCESS) {
			throw std::runtime_error("mesh upload allocation failed");
		}
		//vk::CommandBuffer cmd = uploadContext.uploadCommandBuffer;
		immediate_submit([=](vk::CommandBuffer cmd) {
			vk::BufferCopy copy{};
			copy.dstOffset = 0;
			copy.srcOffset = 0;
			copy.size = bufferSize;
			cmd.copyBuffer(stagingBuffer.buffer, mesh.vertexbuffer.buffer, 1, &copy);
		});

		vmaDestroyBuffer(vallocator, stagingBuffer.buffer, stagingBuffer.allocation);

		//index buffer

		const size_t indexBufferSize = mesh.vertexindices.size() * sizeof(mesh.vertexindices[0]);
		std::cout << "indexsize" << indexBufferSize << " - " <<  mesh.vertexindices.size() << " s:" << sizeof(mesh.vertexindices[0]) << std::endl;
		//vk::BufferCreateInfo bufferinfo{}; lazily reusing old createinfos
		bufferinfo.size = indexBufferSize; //in bytes
		bufferinfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
		
		vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		AllocatedBuffer indexstagingBuffer;

		if (vmaCreateBuffer(
			vallocator,
			reinterpret_cast<VkBufferCreateInfo*>(&bufferinfo),
			&vmaallocinfo,
			reinterpret_cast<VkBuffer*>(&indexstagingBuffer.buffer),
			&indexstagingBuffer.allocation,
			nullptr
			) != VK_SUCCESS) {
			throw std::runtime_error("stagingbuffer allocation failed");
		}

		void* indexdata;
		vmaMapMemory(vallocator, indexstagingBuffer.allocation, &indexdata);
		memcpy(indexdata, mesh.vertexindices.data(), indexBufferSize);
		vmaUnmapMemory(vallocator, indexstagingBuffer.allocation);

		vk::BufferCreateInfo indexbufferinfo{};
		indexbufferinfo.size = indexBufferSize; //in bytes
		indexbufferinfo.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;

		vmaallocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		if (vmaCreateBuffer(
			vallocator,
			reinterpret_cast<VkBufferCreateInfo*>(&indexbufferinfo),
			&vmaallocinfo,
			reinterpret_cast<VkBuffer*>(&mesh.indexbuffer.buffer),
			&mesh.indexbuffer.allocation,
			nullptr
			) != VK_SUCCESS) {
			throw std::runtime_error("indexbuffer upload allocation failed");
		}

		immediate_submit([=](vk::CommandBuffer cmd) {
			vk::BufferCopy copy{};
			copy.dstOffset = 0;
			copy.srcOffset = 0;
			copy.size = indexBufferSize;
			cmd.copyBuffer(indexstagingBuffer.buffer, mesh.indexbuffer.buffer, 1, &copy);
		});

		vmaDestroyBuffer(vallocator, indexstagingBuffer.buffer, indexstagingBuffer.allocation);

	}

	Material* Engine::create_material(vk::Pipeline pipeline,vk::PipelineLayout layout,const std::string& name) {
		Material mat;
		mat.pipeline = pipeline;
		mat.pipelinelayout = layout;
		_materials[name] = mat;
		//std::cout << "material created:" << name << std::endl;
		return &_materials[name];
	}

	Material* Engine::get_material(const std::string& name) {

		auto thing = _materials.find(name);
		if (thing == _materials.end()) {
			std::cout << "materail failed:" << name << std::endl;	
			return nullptr;
		} else{
			//std::cout << "materail gotten:" << name << std::endl;
			return &(*thing).second;
		}

	}

	void Engine::load_material_image(Material* target,std::string name) {

		vk::SamplerCreateInfo sampleinfo{};
		sampleinfo.magFilter = vk::Filter::eNearest;
		sampleinfo.minFilter = vk::Filter::eNearest;
		//sampleinfo.addressModeU = 
		vk::Sampler cleansampler = core->gpudevice.createSampler(sampleinfo);
		samplers.push_back(cleansampler);
		
		vk::DescriptorSetAllocateInfo allocinfo{};
		allocinfo.descriptorPool = descriptorPool;
		allocinfo.descriptorSetCount = 1;
		allocinfo.pSetLayouts = &textureSetLayout;

		target->textureSet = core->gpudevice.allocateDescriptorSets(allocinfo).front();

		vk::DescriptorImageInfo imageBufferInfo{};
		imageBufferInfo.sampler = cleansampler;
		imageBufferInfo.imageView = _textures[name].imageview;
		imageBufferInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		vk::WriteDescriptorSet write{};
		write.dstSet = target->textureSet;
		write.dstBinding = 0;		
		write.descriptorCount = 1;
		write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		write.pImageInfo = &imageBufferInfo; 

		core->gpudevice.updateDescriptorSets(1,&write,0,nullptr);

	}

	CobertMesh* Engine::get_mesh(const std::string& name) {
		auto thing = _meshes.find(name);
		if (thing == _meshes.end()) {
			std::cout << "mesh failed:" << name << std::endl;
			return nullptr;
		} else{
			//std::cout << "mesh gotten:" << name << std::endl;
			return &(*thing).second;
		}
	}

	void Engine::cleanup() {
		core->gpudevice.waitIdle();	
		std::cout << "cleaning up meshes" << std::endl;
	
		for (auto img : loadedimages) {
			vmaDestroyImage(vallocator,static_cast<VkImage>(img.allocatedimage.image),img.allocatedimage.allocation);
			core->gpudevice.destroyImageView(img.imageview, nullptr);
		}

		for (auto sampler : samplers) {
			core->gpudevice.destroySampler(sampler, nullptr);
		}

		vmaDestroyImage(vallocator,static_cast<VkImage>(depthBufferImage.image),depthBufferImage.allocation);
		//vmaDestroyBuffer(vallocator,static_cast<VkBuffer>(mesh.vertexbuffer.buffer),mesh.vertexbuffer.allocation);
		//vmaDestroyBuffer(vallocator,static_cast<VkBuffer>(loadedmesh.vertexbuffer.buffer),loadedmesh.vertexbuffer.allocation);
		vmaDestroyBuffer(vallocator,static_cast<VkBuffer>(_worldParamsBuffer.buffer),_worldParamsBuffer.allocation);
		
		core->gpudevice.destroyDescriptorPool(descriptorPool, nullptr);
		core->gpudevice.destroyDescriptorSetLayout(descriptorSetLayout, nullptr);
		core->gpudevice.destroyDescriptorSetLayout(objectSetLayout, nullptr);
		core->gpudevice.destroyDescriptorSetLayout(textureSetLayout, nullptr);
		
		for (auto fdata : framedatas) {//FRAMEDATA DESTRUCTION
			vmaDestroyBuffer(vallocator,static_cast<VkBuffer>(fdata.camerabuffer.buffer),fdata.camerabuffer.allocation);
			vmaDestroyBuffer(vallocator,static_cast<VkBuffer>(fdata.objectbuffer.buffer),fdata.objectbuffer.allocation);
		}

		for (auto mesh : _meshes) {
			std::cout << "removing mesh " << mesh.first << std::endl;
			vmaDestroyBuffer(vallocator,static_cast<VkBuffer>(mesh.second.vertexbuffer.buffer),mesh.second.vertexbuffer.allocation);
			vmaDestroyBuffer(vallocator,static_cast<VkBuffer>(mesh.second.indexbuffer.buffer),mesh.second.indexbuffer.allocation);
		}

		core->gpudevice.destroyImageView(depthBufferImageView, nullptr);
	
		vmaDestroyAllocator(vallocator);

		std::cout << "cleaning up engine stuff" << std::endl;

		for (auto imageview : swapchainImageViews) {
			core->gpudevice.destroyImageView(imageview, nullptr);
		}

		for (auto framebuffer : swapChainFramebuffer) {
			core->gpudevice.destroyFramebuffer(framebuffer, nullptr);
		}

		for (int i = 0; i < flightFrames; i++) {
			core->gpudevice.destroySemaphore(imageAvailableSem[i],nullptr);
			core->gpudevice.destroySemaphore(renderDoneSem[i],nullptr);
			core->gpudevice.destroyFence(frameFlightFen[i],nullptr);
		}

		core->gpudevice.destroyFence(uploadContext.uploadFence,nullptr);
		core->gpudevice.destroyCommandPool(uploadContext.uploadCommandPool,nullptr);
		
		core->gpudevice.destroyCommandPool(cmdPool,nullptr);

		//core->gpudevice.destroyPipelineLayout(pipelineLayout, nullptr);
		core->gpudevice.destroyPipelineLayout(meshPipelineLayout, nullptr);
		
		core->gpudevice.destroyPipeline(graphicsPipeline, nullptr);
	
		core->gpudevice.destroySwapchainKHR(swapchain, nullptr);
		core->gpudevice.destroyRenderPass(renderpass ,nullptr);
		core->cleanup();
	}

	void Engine::AllocatorCreate() {
		VmaAllocatorCreateInfo allocatorInfo{};
		allocatorInfo.physicalDevice = core->gpu;
		allocatorInfo.device = core->gpudevice;
		allocatorInfo.instance = core->instance;
		vmaCreateAllocator(&allocatorInfo,&vallocator);
	}

	void Engine::SwapchainCreate() {

		core->GetNewSwapchain();
		swapchainDetails = &core->swapchainDetails;
		
		vk::SurfaceCapabilitiesKHR caps = swapchainDetails->capabilities;

		if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			swapchainExtent = caps.currentExtent;
		} else {
			int w,h;
			glfwGetFramebufferSize(core->window,&w,&h);

			swapchainExtent.width = std::clamp((uint32_t) w,caps.minImageExtent.width,caps.maxImageExtent.width);
			swapchainExtent.height = std::clamp((uint32_t) h,caps.minImageExtent.height,caps.maxImageExtent.height);
		}

		uint32_t swapimgcount = caps.minImageCount + 1;
		if (caps.maxImageCount > 0 && caps.maxImageCount < swapimgcount) {
			swapimgcount = caps.maxImageCount;
		}

		auto sharingmode = vk::SharingMode::eExclusive;
		uint32_t indexcount = 0;
		uint32_t queueindicesarray[] = {0,0};

		if (deviceQueueFams->graphics != deviceQueueFams->present) {
			sharingmode = vk::SharingMode::eConcurrent;
			indexcount = 2;
			queueindicesarray[0] = deviceQueueFams->graphics.value();
			queueindicesarray[1] = deviceQueueFams->present.value();
		}

		vk::SwapchainCreateInfoKHR swapCreateInfo{};
		swapCreateInfo.surface = core->surface.get();
		swapCreateInfo.minImageCount = swapimgcount;
		swapCreateInfo.imageFormat = swapchainDetails->surfaceFormat.format;
		swapCreateInfo.imageColorSpace = swapchainDetails->surfaceFormat.colorSpace;
		swapCreateInfo.imageExtent = swapchainExtent;
		swapCreateInfo.imageArrayLayers = 1;
		swapCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
		swapCreateInfo.imageSharingMode = sharingmode;
		swapCreateInfo.queueFamilyIndexCount = indexcount;
		swapCreateInfo.pQueueFamilyIndices = queueindicesarray;
		swapCreateInfo.preTransform = caps.currentTransform;
		swapCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		swapCreateInfo.presentMode = swapchainDetails->presentMode;
		swapCreateInfo.clipped = true;
		swapCreateInfo.oldSwapchain = nullptr;
		

		swapchain = core->gpudevice.createSwapchainKHR(swapCreateInfo);
		std::cout << "swapchain" << std::endl;

		swapchainImages = core->gpudevice.getSwapchainImagesKHR(swapchain);
		std::cout << "got images" << std::endl;

		//image views

		//depth buffer
		vk::Extent3D depthBufferExtent = {
			swapchainExtent.width,
			swapchainExtent.height,
			1
		};

		vk::ImageCreateInfo dimgcreateinfo{};
		dimgcreateinfo.imageType = vk::ImageType::e2D;
		dimgcreateinfo.format = vk::Format::eD32Sfloat;
		dimgcreateinfo.extent = depthBufferExtent;//same size
		dimgcreateinfo.mipLevels = 1;
		dimgcreateinfo.arrayLayers = 1;
		dimgcreateinfo.samples = vk::SampleCountFlagBits::e1;
		dimgcreateinfo.tiling = vk::ImageTiling::eOptimal;
		dimgcreateinfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;



		VmaAllocationCreateInfo dimgallocinfo{};
		dimgallocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		dimgallocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		vmaCreateImage(
			vallocator,
			reinterpret_cast<VkImageCreateInfo*>(&dimgcreateinfo),
			&dimgallocinfo,reinterpret_cast<VkImage*>(&depthBufferImage.image), 
			&depthBufferImage.allocation, 
			nullptr);

		std::cout << "made depthbuffer allocated image" << std::endl;

		vk::ImageViewCreateInfo depthimgviewCreateinfo{};
		depthimgviewCreateinfo.image = depthBufferImage.image;
		depthimgviewCreateinfo.viewType = vk::ImageViewType::e2D;
		depthimgviewCreateinfo.format = vk::Format::eD32Sfloat; //FORMAT
		depthimgviewCreateinfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
		depthimgviewCreateinfo.subresourceRange.baseMipLevel = 0;
		depthimgviewCreateinfo.subresourceRange.levelCount = 1;
		depthimgviewCreateinfo.subresourceRange.baseArrayLayer = 0;
		depthimgviewCreateinfo.subresourceRange.layerCount = 1;

		depthBufferImageView = core->gpudevice.createImageView(depthimgviewCreateinfo);
		std::cout << "made depthbuffer image view" << std::endl;

		//dbi

		swapchainImageViews.resize(swapchainImages.size());

		for (size_t i = 0; i < swapchainImages.size(); i++) {

			vk::ImageViewCreateInfo imgviewCreateinfo{};

			imgviewCreateinfo.image = swapchainImages[i];
			imgviewCreateinfo.viewType = vk::ImageViewType::e2D;
			imgviewCreateinfo.format = swapchainDetails->surfaceFormat.format;

			imgviewCreateinfo.components.r = vk::ComponentSwizzle::eIdentity;
			imgviewCreateinfo.components.g = vk::ComponentSwizzle::eIdentity;
			imgviewCreateinfo.components.b = vk::ComponentSwizzle::eIdentity;
			imgviewCreateinfo.components.a = vk::ComponentSwizzle::eIdentity;
			
			imgviewCreateinfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			imgviewCreateinfo.subresourceRange.baseMipLevel = 0;
			imgviewCreateinfo.subresourceRange.levelCount = 1;
			imgviewCreateinfo.subresourceRange.baseArrayLayer = 0;
			imgviewCreateinfo.subresourceRange.layerCount = 1;

			swapchainImageViews[i] = core->gpudevice.createImageView(imgviewCreateinfo);
			std::cout << "made image view " << i << std::endl;

		}

	}

	void Engine::SwapchainRecreate() {

		for (auto imageview : swapchainImageViews) {
			core->gpudevice.destroyImageView(imageview, nullptr);
		}

		for (auto framebuffer : swapChainFramebuffer) {
			core->gpudevice.destroyFramebuffer(framebuffer, nullptr);
		}

		vmaDestroyImage(vallocator,static_cast<VkImage>(depthBufferImage.image),depthBufferImage.allocation);
		core->gpudevice.destroyImageView(depthBufferImageView, nullptr);
	
		SwapchainCreate();
		FramebufferCreate();

	};

	void Engine::RenderPassCreate() {

		//listen to the vulkan talk about render passes

		//color
		vk::AttachmentDescription colorAttachment{};
		colorAttachment.format = swapchainDetails->surfaceFormat.format;
		colorAttachment.samples = vk::SampleCountFlagBits::e1;
		colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
		colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

		vk::AttachmentReference colAttReference{};
		colAttReference.attachment = 0;
		colAttReference.layout = vk::ImageLayout::eColorAttachmentOptimal;
		//

		//depth
		vk::AttachmentDescription depthAttachment{};
		depthAttachment.format = vk::Format::eD32Sfloat;
		depthAttachment.samples = vk::SampleCountFlagBits::e1;
		depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eClear;
		depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
		depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		vk::AttachmentReference depthAttReference{};
		depthAttReference.attachment = 1;
		depthAttReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		
		//


		vk::SubpassDescription graphicsSubpassDesc{};
		graphicsSubpassDesc.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		graphicsSubpassDesc.colorAttachmentCount = 1;
		graphicsSubpassDesc.pColorAttachments = &colAttReference;
		graphicsSubpassDesc.pDepthStencilAttachment = &depthAttReference;


		vk::SubpassDependency colorSubpassDependency{};
		colorSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		colorSubpassDependency.dstSubpass = 0;
		colorSubpassDependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		colorSubpassDependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		colorSubpassDependency.srcAccessMask = {};
		colorSubpassDependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;	
		//colorSubpassDependency.dependencyFlags = {};

		vk::SubpassDependency depthSubpassDependency{};
		depthSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		depthSubpassDependency.dstSubpass = 0;
		depthSubpassDependency.srcStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
		depthSubpassDependency.dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
		depthSubpassDependency.srcAccessMask = {};
		depthSubpassDependency.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;	
	//	colorSubpassDependency.dependencyFlags = {};


		vk::SubpassDependency dependencies[2] = {colorSubpassDependency,depthSubpassDependency};
		vk::AttachmentDescription descs[2] = {colorAttachment,depthAttachment};

		vk::RenderPassCreateInfo renderpassCreateinfo{};
		renderpassCreateinfo.flags = {},
		renderpassCreateinfo.attachmentCount = 2;
		renderpassCreateinfo.pAttachments = &descs[0];
		renderpassCreateinfo.subpassCount = 1;
		renderpassCreateinfo.pSubpasses = &graphicsSubpassDesc;
		renderpassCreateinfo.dependencyCount = 2,
		renderpassCreateinfo.pDependencies = &dependencies[0];

		renderpass = core->gpudevice.createRenderPass(renderpassCreateinfo);

		std::cout << "renderpass created" << std::endl;

	}

	void Engine::MeshPipelineLayoutCreate() {
		vk::PipelineLayoutCreateInfo meshpipelineinfo{};
		vk::DescriptorSetLayout alllayouts[3] = {descriptorSetLayout,objectSetLayout,textureSetLayout};
		
		vk::PushConstantRange range{};
		range.offset = 0;
		range.size = sizeof(MeshPushConstants);
		range.stageFlags = vk::ShaderStageFlagBits::eVertex;

		meshpipelineinfo.pPushConstantRanges = &range;
		meshpipelineinfo.pushConstantRangeCount = 1;

		meshpipelineinfo.setLayoutCount = 3;
		meshpipelineinfo.pSetLayouts = alllayouts;

		meshPipelineLayout = core->gpudevice.createPipelineLayout(meshpipelineinfo);
	}

	void Engine::DescriptorsCreate() {

		std::vector<vk::DescriptorPoolSize> sizes = {
			{vk::DescriptorType::eUniformBuffer, 10},
			{vk::DescriptorType::eUniformBufferDynamic, 10},
			{vk::DescriptorType::eStorageBuffer, 10},
			{vk::DescriptorType::eCombinedImageSampler, 10},
		};

		const size_t sceneParamBufferSize = flightFrames * pad_uniform_buffer_size(sizeof(GPUWorldData), core->uniformbuffer_alignment);
		_worldParamsBuffer = create_allocatedbuffer(
			sceneParamBufferSize,
			vk::BufferUsageFlagBits::eUniformBuffer,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		vk::DescriptorPoolCreateInfo poolinfo{};
		poolinfo.maxSets = 10;
		poolinfo.poolSizeCount = (uint32_t)sizes.size();
		poolinfo.pPoolSizes = sizes.data();
		descriptorPool = core->gpudevice.createDescriptorPool(poolinfo);

		//bindings
		vk::DescriptorSetLayoutBinding camBufferBinding{};
		camBufferBinding.binding = 0;
		camBufferBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		camBufferBinding.descriptorCount = 1;
		camBufferBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

		vk::DescriptorSetLayoutBinding worldBufferBinding{};
		worldBufferBinding.binding = 1;
		worldBufferBinding.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
		worldBufferBinding.descriptorCount = 1;
		worldBufferBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
	
		vk::DescriptorSetLayoutBinding storageBufferBinding{};
		storageBufferBinding.binding = 0;
		storageBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		storageBufferBinding.descriptorCount = 1;
		storageBufferBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
	
		vk::DescriptorSetLayoutBinding textureBinding{};
		textureBinding.binding = 0;
		textureBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		textureBinding.descriptorCount = 1;
		textureBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		///

		vk::DescriptorSetLayoutBinding allbindings[] = {camBufferBinding,worldBufferBinding};

		vk::DescriptorSetLayoutCreateInfo setinfo{};
		setinfo.bindingCount = 2;
		setinfo.pBindings = allbindings; //&cambufferibnding

		vk::DescriptorSetLayoutCreateInfo setinfoSB{};//storage buffer
		setinfoSB.bindingCount = 1;
		setinfoSB.pBindings = &storageBufferBinding; //&cambufferibnding

		vk::DescriptorSetLayoutCreateInfo setinfotex{};
		setinfotex.bindingCount = 1;
		setinfotex.pBindings = &textureBinding; //&cambufferibnding

		descriptorSetLayout = core->gpudevice.createDescriptorSetLayout(setinfo);
		objectSetLayout = core->gpudevice.createDescriptorSetLayout(setinfoSB);
		textureSetLayout = core->gpudevice.createDescriptorSetLayout(setinfotex);
		
		const int MAX_OBJECTS = 10000;
		for (int i = 0; i < flightFrames; i++) {
			FrameData data;

			data.camerabuffer = create_allocatedbuffer(
				sizeof(GPUCameraData),
				vk::BufferUsageFlagBits::eUniformBuffer,
				VMA_MEMORY_USAGE_CPU_TO_GPU
			);

			data.objectbuffer = create_allocatedbuffer(
				sizeof(GPUObjectData) * MAX_OBJECTS,
				vk::BufferUsageFlagBits::eStorageBuffer,
				VMA_MEMORY_USAGE_CPU_TO_GPU
			);

			vk::DescriptorSetAllocateInfo allocateinfo{};
			allocateinfo.descriptorPool = descriptorPool;
			allocateinfo.descriptorSetCount = 1;
			allocateinfo.pSetLayouts = &descriptorSetLayout;

			vk::DescriptorSetAllocateInfo objectallocateinfo{};
			objectallocateinfo.descriptorPool = descriptorPool;
			objectallocateinfo.descriptorSetCount = 1;
			objectallocateinfo.pSetLayouts = &objectSetLayout;

			data.globalDescriptor = core->gpudevice.allocateDescriptorSets(allocateinfo).front();
			data.objectDescriptor = core->gpudevice.allocateDescriptorSets(objectallocateinfo).front();

			//bufferinfos
			vk::DescriptorBufferInfo camerabufferinfo{};
			camerabufferinfo.buffer = data.camerabuffer.buffer;
			camerabufferinfo.offset = 0;
			camerabufferinfo.range = sizeof(GPUCameraData);

			vk::DescriptorBufferInfo worldbufferinfo{};
			worldbufferinfo.buffer = _worldParamsBuffer.buffer;
			worldbufferinfo.offset = 0; //its eUniformBufferDynamic now //i * pad_uniform_buffer_size(sizeof(GPUWorldData), core->uniformbuffer_alignment);
			worldbufferinfo.range = sizeof(GPUWorldData);

			vk::DescriptorBufferInfo objectbufferinfo{};
			objectbufferinfo.buffer = data.objectbuffer.buffer;
			objectbufferinfo.offset = 0; //its eUniformBufferDynamic now //i * pad_uniform_buffer_size(sizeof(GPUWorldData), core->uniformbuffer_alignment);
			objectbufferinfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;
			//

			//writes
			vk::WriteDescriptorSet camsetwrite{};

			camsetwrite.dstSet = data.globalDescriptor;
			camsetwrite.dstBinding = 0;		
			camsetwrite.descriptorCount = 1;
			camsetwrite.descriptorType = vk::DescriptorType::eUniformBuffer;
			camsetwrite.pBufferInfo = &camerabufferinfo; 

			vk::WriteDescriptorSet worldsetwrite{};

			worldsetwrite.dstSet = data.globalDescriptor;
			worldsetwrite.dstBinding = 1;		
			worldsetwrite.descriptorCount = 1;
			worldsetwrite.descriptorType = vk::DescriptorType::eUniformBufferDynamic; // dynamic;
			worldsetwrite.pBufferInfo = &worldbufferinfo; 

			vk::WriteDescriptorSet objectsetwrite{};

			objectsetwrite.dstSet = data.objectDescriptor;
			objectsetwrite.dstBinding = 0;		
			objectsetwrite.descriptorCount = 1;
			objectsetwrite.descriptorType = vk::DescriptorType::eStorageBuffer; // dynamic;
			objectsetwrite.pBufferInfo = &objectbufferinfo; 
			
			//
			vk::WriteDescriptorSet allwrites[] = {camsetwrite,worldsetwrite,objectsetwrite};
			core->gpudevice.updateDescriptorSets(3,allwrites,0, nullptr);



			framedatas.push_back(data);
		}

	}

 	void Engine::GraphicsPipelineCreate() {
		auto vertexfile = readFile("shaders/vert.spv");
		auto fragfile   = readFile("shaders/frag.spv");

		vk::ShaderModule vertexshader = setupShader(vertexfile,core->gpudevice);
		vk::ShaderModule fragshader   = setupShader(fragfile,core->gpudevice);


		vk::PipelineShaderStageCreateInfo vertStageInfo{};
		vertStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertStageInfo.module = vertexshader;
		vertStageInfo.pName = "main";


		vk::PipelineShaderStageCreateInfo fragStageInfo{};
		fragStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
		fragStageInfo.module = fragshader;
		fragStageInfo.pName = "main";


		vk::PipelineShaderStageCreateInfo pipelineStages[2] = {vertStageInfo,fragStageInfo};
		std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,vk::DynamicState::eScissor};


		vk::PipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.dynamicStateCount = dynamicStates.size();
		dynamicState.pDynamicStates = dynamicStates.data();


		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
		inputAssembly.primitiveRestartEnable = false;

		vk::PipelineViewportStateCreateInfo viewportState{};
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;


		vk::PipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.depthClampEnable = false;
		rasterizer.rasterizerDiscardEnable = false;
		rasterizer.polygonMode = vk::PolygonMode::eFill;
		rasterizer.cullMode = vk::CullModeFlagBits::eBack;	
		rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
		rasterizer.depthBiasEnable = false;
		rasterizer.lineWidth = 1.0f;

		vk::PipelineDepthStencilStateCreateInfo depthstencil{};
		depthstencil.depthTestEnable = true;
		depthstencil.depthWriteEnable = true;
		depthstencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
		depthstencil.depthBoundsTestEnable = false;
		depthstencil.stencilTestEnable = false;

		vk::PipelineMultisampleStateCreateInfo multisampling{};
		multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
		multisampling.sampleShadingEnable = false;
		multisampling.minSampleShading = 1.0f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = false;
		multisampling.alphaToOneEnable = false;


		VertInputDescription mesh_info = Vertex::get_vertex_description();

		vk::PipelineVertexInputStateCreateInfo vertexInputStateInfo{};
		vertexInputStateInfo.vertexBindingDescriptionCount = mesh_info.bindings.size();
		vertexInputStateInfo.vertexAttributeDescriptionCount = mesh_info.attributes.size();

		vertexInputStateInfo.pVertexBindingDescriptions = mesh_info.bindings.data();
		vertexInputStateInfo.pVertexAttributeDescriptions = mesh_info.attributes.data();

		vk::PipelineColorBlendAttachmentState colorblendstate{};
		colorblendstate.blendEnable = true;
		colorblendstate.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha; //alpha
		colorblendstate.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha; //1-alpha
		colorblendstate.colorBlendOp = vk::BlendOp::eAdd; // color*alpha + dstcolor*(1-alpha)
		colorblendstate.srcAlphaBlendFactor = vk::BlendFactor::eOne;
		colorblendstate.dstAlphaBlendFactor = vk::BlendFactor::eZero;
		colorblendstate.alphaBlendOp = vk::BlendOp::eAdd;
		colorblendstate.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB;
	

		vk::PipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.logicOpEnable = false;
		colorBlending.logicOp = vk::LogicOp::eCopy;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorblendstate;

		//vk::PipelineLayoutCreateInfo pipelineCreateinfo{};
		//pipelineCreateinfo.setLayoutCount = 0;
		//pipelineCreateinfo.pushConstantRangeCount = 0;
		
		//pipelineLayout = core->gpudevice.createPipelineLayout(pipelineCreateinfo);

		vk::GraphicsPipelineCreateInfo gpuPipelineCreateinfo{};
		gpuPipelineCreateinfo.stageCount = 2;
		gpuPipelineCreateinfo.pStages = pipelineStages;
		gpuPipelineCreateinfo.pVertexInputState = &vertexInputStateInfo;
		gpuPipelineCreateinfo.pInputAssemblyState = &inputAssembly;
		gpuPipelineCreateinfo.pTessellationState = nullptr;
		gpuPipelineCreateinfo.pViewportState = &viewportState;
		gpuPipelineCreateinfo.pRasterizationState = &rasterizer;
		gpuPipelineCreateinfo.pMultisampleState = &multisampling;
		gpuPipelineCreateinfo.pDepthStencilState = &depthstencil;
		gpuPipelineCreateinfo.pColorBlendState = &colorBlending;
		gpuPipelineCreateinfo.pDynamicState = &dynamicState;
		gpuPipelineCreateinfo.layout = meshPipelineLayout;
		gpuPipelineCreateinfo.renderPass = renderpass;
		gpuPipelineCreateinfo.subpass = 0;
		gpuPipelineCreateinfo.basePipelineHandle = nullptr;
		gpuPipelineCreateinfo.basePipelineIndex = -1;

		vk::Result res;
		std::tie(res,graphicsPipeline) = core->gpudevice.createGraphicsPipeline(nullptr,gpuPipelineCreateinfo);

		core->gpudevice.destroyShaderModule(vertexshader);
		core->gpudevice.destroyShaderModule(fragshader);

		std::cout << "GPU Pipeline created" << std::endl;
	}
	
	void Engine::FramebufferCreate(){

		swapChainFramebuffer.resize(swapchainImageViews.size());

		for (size_t i = 0; i < swapchainImageViews.size(); i++) {

			vk::ImageView atts[] = {swapchainImageViews[i], depthBufferImageView};

			vk::FramebufferCreateInfo framebufferCreateInfo{};
			framebufferCreateInfo.renderPass = renderpass;
			framebufferCreateInfo.attachmentCount = 2;
			framebufferCreateInfo.pAttachments = atts;
			framebufferCreateInfo.width = swapchainExtent.width;
			framebufferCreateInfo.height = swapchainExtent.height;
			framebufferCreateInfo.layers = 1;

			swapChainFramebuffer[i] = core->gpudevice.createFramebuffer(framebufferCreateInfo);
			std::cout << "created framebuffer" << i << std::endl;

		}

	}	

	void Engine::CommandPoolCreate(){

		vk::CommandPoolCreateInfo cmdpoolinfo{};
		cmdpoolinfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
		cmdpoolinfo.queueFamilyIndex = deviceQueueFams->graphics.value();

		cmdPool = core->gpudevice.createCommandPool(cmdpoolinfo);
		std::cout << "command pool created" << std::endl;

		vk::CommandBufferAllocateInfo bufferallocation{};
		bufferallocation.commandPool = cmdPool;
		bufferallocation.level = vk::CommandBufferLevel::ePrimary;
		bufferallocation.commandBufferCount = flightFrames;

		cmdBuffer = core->gpudevice.allocateCommandBuffers(bufferallocation);

		std::cout << "allocated buffer" << std::endl;

		vk::CommandPoolCreateInfo uploadcmdpoolinfo{};
		uploadcmdpoolinfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
		uploadcmdpoolinfo.queueFamilyIndex = deviceQueueFams->graphics.value();
		uploadContext.uploadCommandPool = core->gpudevice.createCommandPool(uploadcmdpoolinfo);
		
		vk::CommandBufferAllocateInfo uploadcmdallocate{};
		uploadcmdallocate.commandPool = cmdPool;
		uploadcmdallocate.level = vk::CommandBufferLevel::ePrimary;
		uploadcmdallocate.commandBufferCount = 1;
		
		uploadContext.uploadCommandBuffer = core->gpudevice.allocateCommandBuffers(uploadcmdallocate).front();


		std::cout << "upload buffers done" << std::endl;

	}	


	void Engine::SyncObjectsCreate(){
		vk::SemaphoreCreateInfo seminfo{};
		vk::FenceCreateInfo fenceinfo(vk::FenceCreateFlagBits::eSignaled);

		imageAvailableSem.resize(flightFrames);
		renderDoneSem.resize(flightFrames);
		frameFlightFen.resize(flightFrames);

		for (int i = 0; i < flightFrames; i++) {
			imageAvailableSem[i] = core->gpudevice.createSemaphore(seminfo);
			renderDoneSem[i] = core->gpudevice.createSemaphore(seminfo);
			frameFlightFen[i] = core->gpudevice.createFence(fenceinfo);
		}

		std::cout << "semaphroes crated" << std::endl;

		vk::FenceCreateInfo fenceinfo2{};
		uploadContext.uploadFence = core->gpudevice.createFence(fenceinfo2);

		std::cout << "context created" << std::endl;
	}	

	void Engine::draw_world(vk::CommandBuffer*& cmdBufferCurrent, WorldObject* first, int count, float globaltick) {

		float movex = 0;
		float movez = 0;

		if (keyboard[GLFW_KEY_W]) {
		//	std::cout << "w" << std::endl;
			movez = 0.03f;
		} else if (keyboard[GLFW_KEY_S]) {
		//	std::cout << "s" << std::endl;
			movez = -0.03f;
		}

		if (keyboard[GLFW_KEY_A]) {
		//	std::cout << "a" << std::endl;
			movex = 0.03f;
		} else if (keyboard[GLFW_KEY_D]) {
		//	std::cout << "d" << std::endl;
			movex = -0.03f;
		}

		camerarotx += mousedeltay/2;
		cameraroty += mousedeltax/2;

		mousedeltay = 0;
		mousedeltax = 0;

		if (camerarotx > 80) {
			camerarotx = 80;
		} else if (camerarotx < -80) {
			camerarotx = -80;
		}

		camMat = glm::rotate(glm::mat4{1.0f}, glm::radians((float) camerarotx), glm::vec3(1,0,0));
		camMat = glm::rotate(camMat, glm::radians((float) cameraroty), glm::vec3(0,1,0));

		//glm::mat4 movemat = glm::rotate(glm::mat4{1.0f}, glm::radians((float) cameraroty), glm::vec3(0,1,0));
		glm::vec3 fwdvec = glm::normalize(glm::inverse(camMat)[2]);
		glm::vec3 rightvec = glm::normalize(glm::inverse(camMat)[0]); 

		camPos = camPos + (fwdvec*(movez*3.0f)) + (rightvec*(movex*3.0f));

		glm::mat4 view = glm::translate(camMat, camPos);
		glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
		projection[1][1] *= -1;

		GPUCameraData camData;
		camData.proj = projection;
		camData.view = view;
		camData.viewproj = projection * view;

		//copy gpudata to buffer
		FrameData fdata = framedatas[currentFrame];

		void* data;
		vmaMapMemory(vallocator,fdata.camerabuffer.allocation, &data); //map memory from allocation to data
		memcpy(data, &camData, sizeof(GPUCameraData));  //copy camdata to data allocation
		vmaUnmapMemory(vallocator,fdata.camerabuffer.allocation); //unmmap memoory from data;

		_worldParams.ambientColor = {sin(globaltick),sin(globaltick*1.3f),cos(globaltick),1};
		
		char* sceneData;
		vmaMapMemory(vallocator,_worldParamsBuffer.allocation, (void**)&sceneData);

		sceneData += pad_uniform_buffer_size(sizeof(GPUWorldData), core->uniformbuffer_alignment) * currentFrame;
		memcpy(sceneData, &_worldParams, sizeof(GPUWorldData));
		vmaUnmapMemory(vallocator,_worldParamsBuffer.allocation);

		void* objectData;
		vmaMapMemory(vallocator, fdata.objectbuffer.allocation, &objectData);
		GPUObjectData* objectSSBO = (GPUObjectData*)objectData;
		for (int i = 0; i < count; i++) {
			WorldObject& object = first[i];
			objectSSBO[i].objectMatrix = object.transform;
		} 

		vmaUnmapMemory(vallocator,fdata.objectbuffer.allocation);

		CobertMesh* lastmesh;
		Material* lastmat;

		for (int i = 0; i < count; i++) {
			WorldObject& obj = first[i];

			if (lastmat != obj.material) {
				cmdBufferCurrent->bindPipeline(vk::PipelineBindPoint::eGraphics, obj.material->pipeline);
				lastmat = obj.material;

				if (obj.material->textureSet) {
					cmdBufferCurrent->bindDescriptorSets(
						vk::PipelineBindPoint::eGraphics, 
						obj.material->pipelinelayout,
						2,1,
						&obj.material->textureSet,
						0,
						nullptr
					);
				}

				uint32_t uniform_offset = currentFrame * pad_uniform_buffer_size(sizeof(GPUWorldData), core->uniformbuffer_alignment);
				//bind the desc set to the new pipeline, if switched
				cmdBufferCurrent->bindDescriptorSets(
					vk::PipelineBindPoint::eGraphics, 
					obj.material->pipelinelayout,
					0,1,
					&fdata.globalDescriptor,
					1,
					&uniform_offset
					);

				cmdBufferCurrent->bindDescriptorSets(
					vk::PipelineBindPoint::eGraphics, 
					obj.material->pipelinelayout,
					1,1,
					&fdata.objectDescriptor,
					0,
					nullptr
					);
			}

			//glm::mat4 model = obj.transform;//glm::rotate(glm::mat4{ 1.0f }, glm::radians(globaltick), glm::vec3(0,1,0));
			//glm::mat4 mesh_matrix = projection * view * model;

			MeshPushConstants constants;
			constants.render_matrix = obj.transform;
			constants.data[0] = globaltick;

			cmdBufferCurrent->pushConstants(obj.material->pipelinelayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(MeshPushConstants), &constants);
			//

			if (obj.mesh != lastmesh) {
				vk::DeviceSize offset = 0;
				cmdBufferCurrent->bindVertexBuffers(0, 1, &obj.mesh->vertexbuffer.buffer, &offset);
				cmdBufferCurrent->bindIndexBuffer(obj.mesh->indexbuffer.buffer, offset, vk::IndexType::eUint32);
				lastmesh = obj.mesh;
			}

			//cmdBufferCurrent->draw(obj.mesh->vertices.size(),1,0,i);
			cmdBufferCurrent->drawIndexed(obj.mesh->vertexindices.size(),1,0,0,i);
		}
	}

	void Engine::EngineStep(){

		if (oldcursorlock != cursorlocked) {
			oldcursorlock = cursorlocked;

			if (cursorlocked == true) {
				glfwSetInputMode(core->window, GLFW_CURSOR,GLFW_CURSOR_DISABLED);
			} else {
				glfwSetInputMode(core->window, GLFW_CURSOR,GLFW_CURSOR_NORMAL);
			}
		}

		//drawing time
		globaltick += 0.1f;

		[[maybe_unused]] vk::Result res;

		res = core->gpudevice.waitForFences(1,&frameFlightFen[currentFrame], true, UINT64_MAX);

		uint32_t nextimageindex = 0;
		res = core->gpudevice.acquireNextImageKHR(
			swapchain,
			UINT64_MAX,
			imageAvailableSem[currentFrame],
			VK_NULL_HANDLE,
			&nextimageindex
		);

		if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR || windowCurrentlyResized == true) {
			windowCurrentlyResized = false;
			SwapchainRecreate();
			return;
		} else if (res != vk::Result::eSuccess) {
			throw std::runtime_error("failed to get good swapchain image");
		}

		vk::CommandBuffer* cmdBufferCurrent = &cmdBuffer[currentFrame];

		res = core->gpudevice.resetFences(1, &frameFlightFen[currentFrame]);
		cmdBufferCurrent->reset();

		//recording buffer
		vk::CommandBufferBeginInfo begininfo{};
		cmdBufferCurrent->begin(begininfo);

		vk::Rect2D rect;
		rect.extent = swapchainExtent;

		vk::ClearValue clearcol{};
		clearcol.color = vk::ClearColorValue(std::array<float, 4>({{0.1f, 0.1f, 0.1f, 1.0f}}));

		vk::ClearValue depthclearcol{};
		depthclearcol.depthStencil.depth = 1.0f;

		vk::ClearValue clearvalues[2] = {clearcol,depthclearcol};

		
		vk::RenderPassBeginInfo renderpassinfo{};
		renderpassinfo.renderPass = renderpass;
		renderpassinfo.framebuffer = swapChainFramebuffer[nextimageindex];
		renderpassinfo.renderArea = rect;
		renderpassinfo.clearValueCount = 2;
		renderpassinfo.pClearValues = &clearvalues[0];

		cmdBufferCurrent->beginRenderPass(renderpassinfo,vk::SubpassContents::eInline);
		//cmdBufferCurrent->bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

		vk::Viewport vp;
		vp.width = (float) swapchainExtent.width;
		vp.height = (float) swapchainExtent.height;
		vp.maxDepth = 1.0f;
		cmdBufferCurrent->setViewport(0, 1, &vp);

		vk::Rect2D scissorrect = rect;
		cmdBufferCurrent->setScissor(0, 1, &scissorrect);

		//CobertMesh* desiredmesh = &loadedmesh;

		draw_world(cmdBufferCurrent, _world.data(), _world.size(),globaltick);

		//vk::DeviceSize offset = 0;
		//cmdBufferCurrent->bindVertexBuffers(0, 1, &desiredmesh->vertexbuffer.buffer, &offset);

		//computing push constants

		/*
		glm::vec3 camPos = {0.f,0.f,-2.f};
		glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);

		glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
		projection[1][1] *= -1;

		glm::mat4 model = glm::rotate(glm::mat4{ 1.0f }, glm::radians(globaltick), glm::vec3(0,1,0));

		glm::mat4 mesh_matrix = projection * view * model;

		MeshPushConstants constants;
		constants.render_matrix = mesh_matrix;
		constants.data[0] = globaltick;
		cmdBufferCurrent->pushConstants(meshPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(MeshPushConstants), &constants);
		//

		cmdBufferCurrent->draw(desiredmesh->vertices.size(),1,0,0);
		*/


		cmdBufferCurrent->endRenderPass();
		cmdBufferCurrent->end();
		//end of recording buffer

		vk::Semaphore wsems[] = {imageAvailableSem[currentFrame]};
		vk::Semaphore signalsems[] = {renderDoneSem[currentFrame]};;
		vk::PipelineStageFlags wstages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

		vk::SubmitInfo sinfo{};
		sinfo.waitSemaphoreCount = 1;
		sinfo.pWaitSemaphores = wsems;
		sinfo.pWaitDstStageMask = wstages;

		sinfo.commandBufferCount = 1;
		sinfo.pCommandBuffers = cmdBufferCurrent;

		sinfo.signalSemaphoreCount = 1;
		sinfo.pSignalSemaphores = signalsems;

		res = core->graphicsQueue.submit(1, &sinfo, frameFlightFen[currentFrame]);


		vk::PresentInfoKHR presentinfo{};
		presentinfo.waitSemaphoreCount = 1;
		presentinfo.pWaitSemaphores = signalsems;
		presentinfo.swapchainCount = 1;
		presentinfo.pSwapchains = &swapchain;
		presentinfo.pImageIndices = &nextimageindex;

		res = core->presentQueue.presentKHR(presentinfo);
		currentFrame = (currentFrame + 1) % flightFrames;

	}
	//

	bool load_image_file(Engine engine, const char* file, AllocatedImage& out) {
		int texwidth,texheight,texchannels;
		stbi_uc* pixels = stbi_load(file, &texwidth, &texheight, &texchannels, STBI_rgb_alpha);
		if (!pixels) {
			std::cerr << "failed to load in image you ididot " << file << std::endl;
			return false;
		}

		void* pixel_ptr = pixels;
		vk::DeviceSize imagesize = texwidth * texheight * 4; // in bytes everyfing in bytes
		vk::Format imageformat = vk::Format::eB8G8R8A8Srgb;

		vk::BufferCreateInfo bufferinfo{};
		bufferinfo.size = imagesize; //in bytes
		bufferinfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

		AllocatedBuffer stagingBuffer;

		VmaAllocationCreateInfo allocinfo{};
		allocinfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		if (vmaCreateBuffer(
			engine.vallocator,
			reinterpret_cast<VkBufferCreateInfo*>(&bufferinfo),
			&allocinfo,
			reinterpret_cast<VkBuffer*>(&stagingBuffer.buffer),
			&stagingBuffer.allocation,
			nullptr
			) != VK_SUCCESS) {
			throw std::runtime_error("image stagingbuffer allocation failed");
		}

		void* data;
		vmaMapMemory(engine.vallocator, stagingBuffer.allocation, &data);
		memcpy(data, pixel_ptr, static_cast<size_t>(imagesize));
		vmaUnmapMemory(engine.vallocator, stagingBuffer.allocation);
		stbi_image_free(pixels);


		vk::Extent3D imageExtent;
		imageExtent.width = static_cast<uint32_t>(texwidth);
		imageExtent.height = static_cast<uint32_t>(texheight);
		imageExtent.depth = 1;

		vk::ImageCreateInfo imageinfo{};
		imageinfo.imageType = vk::ImageType::e2D;
		imageinfo.format = imageformat;
		imageinfo.extent = imageExtent;
		imageinfo.mipLevels = 1;
		imageinfo.arrayLayers = 1;
		imageinfo.samples = vk::SampleCountFlagBits::e1;
		imageinfo.tiling = vk::ImageTiling::eOptimal;
		imageinfo.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

		AllocatedImage newimg;

		allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		vmaCreateImage(
			engine.vallocator,
			reinterpret_cast<VkImageCreateInfo*>(&imageinfo),
			&allocinfo,reinterpret_cast<VkImage*>(&newimg.image), 
			&newimg.allocation, 
			nullptr);

		engine.immediate_submit([=](vk::CommandBuffer cmd) {

			vk::ImageSubresourceRange range;
			range.aspectMask = vk::ImageAspectFlagBits::eColor;
			range.baseMipLevel = 0;
			range.levelCount = 1;
			range.baseArrayLayer = 0;
			range.layerCount = 1;

			vk::ImageMemoryBarrier transfer_barrier{};
			transfer_barrier.oldLayout = vk::ImageLayout::eUndefined;
			transfer_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			transfer_barrier.image = newimg.image;
			transfer_barrier.subresourceRange = range;

			//transfer_barrier.srcAccessMask = 0;
			transfer_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::PipelineStageFlagBits::eTransfer,
				{},
				0,
				nullptr,
				0,
				nullptr,
				1,
				&transfer_barrier);

			vk::BufferImageCopy copyRegion{};
			copyRegion.bufferOffset = 0;
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;

			copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
			copyRegion.imageSubresource.mipLevel = 0;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageExtent = imageExtent;

			cmd.copyBufferToImage(stagingBuffer.buffer, newimg.image, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion);
		});



		vmaDestroyBuffer(engine.vallocator, stagingBuffer.buffer, stagingBuffer.allocation);
		out = newimg;

		return true;

	}

	void start() {
		std::cout << "starting" << std::endl;
		
		Engine eng(true);

		//VulkanCore core((uint32_t) 500,(uint32_t) 600);
		
		//while (!glfwWindowShouldClose(core.window)) {
		//	glfwPollEvents();
		//}
		
		//core.cleanup();
	}

}