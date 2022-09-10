#include "vengine.hpp"

const std::vector<const char*> valLayers = {
	"VK_LAYER_KHRONOS_validation"
};
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};


#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

const int MAX_FRAMES_IN_FLIGHT = 2;

VEngine::VEngine(int W,int H) 
	:WIDTH(W),HEIGHT(H)
{}

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

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
    

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    
    return VK_FALSE;
}

void vkdo(VkResult x, const char* error) {
	if (x != VK_SUCCESS) {throw std::runtime_error(error);}
}

//void VEngine::resizeCallback(GLFWwindow* win, int width, int height) {
//	auto me = reinterpret_cast<VEngine*>(glfwGetWindowUserPointer(win));
//	me->windowCurrentlyResized = true;
//}

static void test(GLFWwindow* win, int width, int height) { 
	auto me = reinterpret_cast<VEngine*>(glfwGetWindowUserPointer(win));
	me->windowCurrentlyResized = true;
	//std::cout << "yeah" << std::endl;
}

void VEngine::start() {
	std::cout << "starting engine... Width: " << WIDTH << " Height: " << HEIGHT << std::endl;

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	mainwindow = glfwCreateWindow(WIDTH, HEIGHT, "Engine", nullptr, nullptr);
	glfwSetWindowUserPointer(mainwindow,this);
	glfwSetFramebufferSizeCallback(mainwindow,test);

	vertices.resize(4);
	
	vertices[0].Position = glm::vec2(-0.5f,-0.5f);
	vertices[0].Colour = glm::vec3(1.0f,0.0f,0.0f);
	
	vertices[1].Position = glm::vec2(0.5f,-0.5f);
	vertices[1].Colour = glm::vec3(0.0f,1.0f,0.0f);
	
	vertices[2].Position = glm::vec2(0.5f,0.5f);
	vertices[2].Colour = glm::vec3(0.0f,0.0f,1.0f);
	
	vertices[3].Position = glm::vec2(-0.5f,0.5f);
	vertices[3].Colour = glm::vec3(1.0f,1.0f,1.0f);

	vertex_indices.resize(6);
	vertex_indices = {0,1,2,2,3,0};

	setupVulkan(); 

}

void VEngine::clearSwapchain() {
	for (auto thing : swapChainImageViews) {
		gpudevice.destroyImageView(thing,nullptr);
	}

	for (auto thing : swapChainFramebuffer) {
		gpudevice.destroyFramebuffer(thing,nullptr);
	}
}

void VEngine::cleanup() {
	//vkDestroyInstance(vulkaninstance,nullptr);
	std::cout << "cleaning up" << std::endl;

	gpudevice.destroyBuffer(vertexbuffer,nullptr);
	gpudevice.freeMemory(vertexbuffermemory,nullptr);
	gpudevice.destroyBuffer(indexbuffer,nullptr);
	gpudevice.freeMemory(indexbuffermemory,nullptr);

	gpudevice.destroyDescriptorSetLayout(descriptorSetLayout,nullptr);

	clearSwapchain();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		gpudevice.destroySemaphore(imageAvailableSem[i],nullptr);
		gpudevice.destroySemaphore(renderDoneSem[i],nullptr);
		gpudevice.destroyFence(frameFlightFen[i],nullptr);
	}

	//gpudevice.destroySemaphore(imageAvailableSem,nullptr);
	//gpudevice.destroySemaphore(renderDoneSem,nullptr);
	//gpudevice.destroyFence(frameFlightFen,nullptr);

	gpudevice.destroyPipeline(graphicsPipeline,nullptr);

	gpudevice.destroyCommandPool(cmdPool,nullptr);

	gpudevice.destroySwapchainKHR(swapChain,nullptr);
	gpudevice.destroyRenderPass(renderpass,nullptr);
	gpudevice.destroyPipelineLayout(pipelineLayout,nullptr);
	
	vulkaninstance.destroySurfaceKHR(csurface,nullptr);
	vulkaninstance.destroyDebugUtilsMessengerEXT(debugmessenger,nullptr,instanceLoader);
	//vulkaninstance.destroyDevice(gpudevice,nullptr);
	gpudevice.destroy();
	vulkaninstance.destroy();

	glfwDestroyWindow(mainwindow);
	glfwTerminate();

}

std::vector<const char*> getRequiredExtensions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> allextensions(glfwExtensions,glfwExtensions+glfwExtensionCount);

	for (const char* extention: allextensions) {
		std::cout << extention << std::endl;
	}

	if (enableValidationLayers == true) {
		allextensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return allextensions;
}

void setupDebugMessenger(vk::Instance& instance, vk::DebugUtilsMessengerEXT& debugmessengerref,vk::DispatchLoaderDynamic& instanceLoader) {


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

	vk::DebugUtilsMessengerCreateInfoEXT createinfo({},
		severities,
		types,
		&debugCallback
	);

	instanceLoader = vk::DispatchLoaderDynamic(instance, vkGetInstanceProcAddr);
	//instance loader just makes everything so much easier, instead of having to go through 
	//the pain that comes with getting the instance proc address, and loading in extentions by hand,
	//you can easily just use the DispatchLoaderDynamic to load things dynamically for you


	debugmessengerref = instance.createDebugUtilsMessengerEXT(createinfo, nullptr, instanceLoader);

	std::cout << "debug callback created" << std::endl;
}

SwapChainSupportDetails getSwapchainDetails(vk::PhysicalDevice* d, vk::UniqueSurfaceKHR* surface) {

	SwapChainSupportDetails details;
	auto surf = surface->get();

	details.capabilities = d->getSurfaceCapabilitiesKHR(surf);

	std::vector<vk::SurfaceFormatKHR> formats = d->getSurfaceFormatsKHR(surf);  	 
	std::vector<vk::PresentModeKHR> presentModes = d->getSurfacePresentModesKHR(surf);  	
	
	if (formats.empty() || presentModes.empty()) {
		details.supported = false; 
		return details;
	} // didnt return what was needed so just terminate here

	details.supported = true;

	for (const auto& availableformat : formats) {
		if (availableformat.format == vk::Format::eB8G8R8A8Srgb && availableformat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			details.surfaceFormat = availableformat;
			break;
		}  
	}

	for (const auto& availablepresent : presentModes) {
		if (availablepresent == vk::PresentModeKHR::eMailbox) {
			details.presentMode = availablepresent;
			break;
		}
	}

	return details;
}

bool checkDeviceSupportsExtentions(vk::PhysicalDevice* d) {
	std::vector<vk::ExtensionProperties> dextensions = d->enumerateDeviceExtensionProperties();
	
	std::set<std::string> reqextentions(deviceExtensions.begin(),deviceExtensions.end());

	for (const auto& extension: dextensions) {
		reqextentions.erase(extension.extensionName);
		std::cout << "extension" << extension.extensionName << std::endl;
	}

	if (reqextentions.empty()) std::cout << "success" << std::endl;

	return reqextentions.empty();
}

QueueFamilyIndices getQueueFamsFromDevice(vk::PhysicalDevice* d, vk::UniqueSurfaceKHR* surface) {
	QueueFamilyIndices indices;
	//get list of queue families
	//pick out graphics family and present family
	//uint32_t qfc = 0; 
	std::vector<vk::QueueFamilyProperties> queueFams = d->getQueueFamilyProperties();

	int i = 0;
	for (const auto& family : queueFams) {

		std::cout << vk::to_string(family.queueFlags) << std::endl;
		if (family.queueFlags & vk::QueueFlagBits::eGraphics) {
			std::cout << "found graphics" << std::endl;
			indices.graphicsFamily = i;
		} 

		if (d->getSurfaceSupportKHR(i, surface->get())) {
			std::cout << "found present > " << i << std::endl;
			indices.presentFamily = i;
		}

		if (indices.isComplete()) break;
		i++;

	}

	return indices;
}

void VEngine::createSwapchain(bool refreshDSD) {

	//get correct width and height for the swap chain

	if (refreshDSD == true) {
		deviceSwapchainDetails = getSwapchainDetails(&gpu, &surface);
	//for recreational purposes
	}

	vk::SurfaceCapabilitiesKHR caps = deviceSwapchainDetails.capabilities;

	if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {

		swapChainExtent = caps.currentExtent;

	} else {

		int width,height;
		glfwGetFramebufferSize(mainwindow, &width, &height);

		swapChainExtent.width = std::clamp(static_cast<uint32_t>(width),caps.minImageExtent.width,caps.maxImageExtent.width);
		swapChainExtent.height = std::clamp(static_cast<uint32_t>(height),caps.minImageExtent.height,caps.maxImageExtent.height);
		
	}


	//time 2 make swap chaen

	uint32_t simageCount = caps.minImageCount + 1;

	if (caps.maxImageCount > 0 && caps.maxImageCount < simageCount) {
		simageCount = caps.maxImageCount;
	}

	bool graphicsPresentSame = (deviceQueueFams.graphicsFamily == deviceQueueFams.presentFamily);


	auto sharingmode = vk::SharingMode::eExclusive;
	uint32_t indexcount = 0;
	uint32_t qfi[] = {0,0};

	if (!graphicsPresentSame) {
		sharingmode = vk::SharingMode::eConcurrent;
		indexcount = 2;
		qfi[0] = deviceQueueFams.graphicsFamily.value();
		qfi[1] = deviceQueueFams.presentFamily.value();
		
		//qfi = {deviceQueueFams.graphicsFamily.value(),deviceQueueFams.presentFamily.value()};
	}

	vk::SwapchainCreateInfoKHR swapchainCreateinfo(

		vk::SwapchainCreateFlagsKHR(),
		surface.get(),
		simageCount,
		deviceSwapchainDetails.surfaceFormat.format,
		deviceSwapchainDetails.surfaceFormat.colorSpace,
		swapChainExtent,
		1,
		vk::ImageUsageFlagBits::eColorAttachment,
        sharingmode,
		indexcount,
		qfi,
		caps.currentTransform,
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		deviceSwapchainDetails.presentMode,
		true,
		nullptr

	);
	/*
	swapchainCreateinfo.imageSharingMode = sharingmode;
	swapchainCreateinfo.queueFamilyIndexCount = indexcount;
	swapchainCreateinfo.pQueueFamilyIndices = qfi;

	swapchainCreateinfo.preTransform = caps.currentTransform;
	swapchainCreateinfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	swapchainCreateinfo.presentMode = deviceSwapchainDetails.presentMode,
	swapchainCreateinfo.clipped = VK_TRUE;
	swapchainCreateinfo.oldSwapchain = nullptr;*/

	swapChain = gpudevice.createSwapchainKHR(swapchainCreateinfo);
	std::cout << "created swapchain" << std::endl;

	swapChainImages = gpudevice.getSwapchainImagesKHR(swapChain);
	std::cout << "got swapchain images" << std::endl;

}

void VEngine::createImageviews(){

	swapChainImageViews.resize(swapChainImages.size());

	//image views allows a door to look at a vkimage on a screen

	for (size_t i = 0; i < swapChainImages.size(); i++) {

		vk::ImageViewCreateInfo imgviewinfo{};

		imgviewinfo.image = swapChainImages[i];
		imgviewinfo.viewType = vk::ImageViewType::e2D;
		imgviewinfo.format = deviceSwapchainDetails.surfaceFormat.format;

		imgviewinfo.components.r = vk::ComponentSwizzle::eIdentity;
		imgviewinfo.components.g = vk::ComponentSwizzle::eIdentity;
		imgviewinfo.components.b = vk::ComponentSwizzle::eIdentity;
		imgviewinfo.components.a = vk::ComponentSwizzle::eIdentity;

		imgviewinfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		imgviewinfo.subresourceRange.baseMipLevel = 0;
		imgviewinfo.subresourceRange.levelCount = 1;
		imgviewinfo.subresourceRange.baseArrayLayer = 0;
		imgviewinfo.subresourceRange.layerCount = 1;

		swapChainImageViews[i] = gpudevice.createImageView(imgviewinfo);
		std::cout << "made image view " << i << std::endl;

	}

}

void VEngine::createFramebuffer(){

	swapChainFramebuffer.resize(swapChainImageViews.size());

	for (size_t i = 0; i < swapChainImageViews.size(); i++) {

		vk::ImageView atts[] = {swapChainImageViews[i]};

		vk::FramebufferCreateInfo framebufferinfo(
			{},

			renderpass,
			1,
			atts,
			swapChainExtent.width,
			swapChainExtent.height,
			1

		);

		swapChainFramebuffer[i] = gpudevice.createFramebuffer(framebufferinfo);
		std::cout << "create framebuffer " << i << std::endl;

	}

}

void VEngine::createRenderpass() {

	vk::AttachmentDescription colorAttachment(

		{},
		deviceSwapchainDetails.surfaceFormat.format,
		vk::SampleCountFlagBits::e1,

		vk::AttachmentLoadOp::eClear, //img
		vk::AttachmentStoreOp::eStore,

		vk::AttachmentLoadOp::eDontCare, //stencil, not in use right now so no need to worry about it
		vk::AttachmentStoreOp::eDontCare,

		vk::ImageLayout::eUndefined,
		vk::ImageLayout::ePresentSrcKHR

	);

	vk::AttachmentReference colorAttachmentReference(
		0,
		vk::ImageLayout::eColorAttachmentOptimal
	);

	vk::SubpassDescription subpass(
		{},
		vk::PipelineBindPoint::eGraphics
	);

	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;

	vk::SubpassDependency subpassdependency(
		VK_SUBPASS_EXTERNAL,
		0,

		vk::PipelineStageFlagBits::eColorAttachmentOutput, //is to do with color attachment stage SRC
		vk::PipelineStageFlagBits::eColorAttachmentOutput, //is to do with color attachment stage DST
		
		{}, //idk SRC
		vk::AccessFlagBits::eColorAttachmentWrite, //allow when its able to write DST

		{}
	);

	vk::RenderPassCreateInfo renderpassCreateinfo(
		{},
		1,//attachments
		&colorAttachment,
		1,//subpasses
		&subpass,
		1,//dependencies
		&subpassdependency
	);

	renderpass = gpudevice.createRenderPass(renderpassCreateinfo);

	std::cout << "created render pass successfully" << std::endl;

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

uint32_t findMemtype(uint32_t typeFilter, vk::MemoryPropertyFlags properties, vk::PhysicalDevice gpu) {
	//uint32_t memtypeIndex = 0;

	vk::PhysicalDeviceMemoryProperties physicalMemory = gpu.getMemoryProperties();
	for (uint32_t i = 0; i < physicalMemory.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (physicalMemory.memoryTypes[i].propertyFlags & properties) == properties) { // what the fuck is this line
			return i;
		}
	}

	throw std::runtime_error("could not get memory type");
}

void createBuffer(
		vk::DeviceSize size,
		vk::BufferUsageFlags usage,
		vk::MemoryPropertyFlags properties,

		vk::Buffer& buffer,
		vk::DeviceMemory& dmemory,

		vk::PhysicalDevice gpu,
		vk::Device gpudevice

	)   {

	vk::BufferCreateInfo bufferinfo(
		{},

		size,
		usage,
		vk::SharingMode::eExclusive
	);

	buffer = gpudevice.createBuffer(bufferinfo);
	vk::MemoryRequirements memReqs = gpudevice.getBufferMemoryRequirements(buffer);

	vk::MemoryAllocateInfo allocinfo(
		//{},
		memReqs.size,
		findMemtype(memReqs.memoryTypeBits, properties, gpu)
	);

	dmemory = gpudevice.allocateMemory(allocinfo);
	gpudevice.bindBufferMemory(buffer,dmemory,0);

}

void VEngine::copyBuffer(vk::Buffer source, vk::Buffer destination, vk::DeviceSize size) {

	vk::CommandBufferAllocateInfo bufferinfo(	
		cmdPool,
		vk::CommandBufferLevel::ePrimary,
		1
	);

	vk::CommandBuffer tempBuffer = gpudevice.allocateCommandBuffers(bufferinfo).front();

	vk::CommandBufferBeginInfo begininfo{};
	begininfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	tempBuffer.begin(begininfo);

	vk::BufferCopy copyBufferRegion(
		0,
		0,
		size
	);

	tempBuffer.copyBuffer(source, destination, 1, &copyBufferRegion);

	tempBuffer.end();

	vk::SubmitInfo submitinfo{};
	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &tempBuffer;

	graphicsQueue.submit(submitinfo);
	graphicsQueue.waitIdle();

	gpudevice.freeCommandBuffers(cmdPool, 1, &tempBuffer);

}

void VEngine::createVertBuffer() {
	vk::DeviceSize buffersize = sizeof(vertices[0]) * vertices.size();

	vk::Buffer stagingBuffer;
	vk::DeviceMemory stagingBufferMemory;

	createBuffer(
		buffersize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,

		stagingBuffer,
		stagingBufferMemory,

		//required
		gpu,
		gpudevice
	);
	//map
	void* data = gpudevice.mapMemory(stagingBufferMemory, 0, buffersize);
	//loading vertices into memory
	//hello
	memcpy(data, vertices.data(), (size_t) buffersize);
	//unmapping the memory noob we are finished now
	gpudevice.unmapMemory(stagingBufferMemory);

	createBuffer(
		buffersize,
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal,

		vertexbuffer,
		vertexbuffermemory,

		//required
		gpu,
		gpudevice
	);

	copyBuffer(stagingBuffer, vertexbuffer ,buffersize);

	gpudevice.destroyBuffer(stagingBuffer,nullptr);
	gpudevice.freeMemory(stagingBufferMemory,nullptr);

}

void VEngine::createIndexBuffer() {
	vk::DeviceSize indexbuffersize = sizeof(vertex_indices[0]) * vertex_indices.size();

	vk::Buffer stagingBuffer;
	vk::DeviceMemory stagingBufferMemory;

	createBuffer(
		indexbuffersize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,

		stagingBuffer,
		stagingBufferMemory,

		//required
		gpu,
		gpudevice
	);
	//map
	void* data = gpudevice.mapMemory(stagingBufferMemory, 0, indexbuffersize);
	//loading vertices into memory
	//hello
	memcpy(data, vertex_indices.data(), (size_t) indexbuffersize);
	//unmapping the memory noob we are finished now
	gpudevice.unmapMemory(stagingBufferMemory);

	createBuffer(
		indexbuffersize,
		vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal,

		indexbuffer,
		indexbuffermemory,

		//required
		gpu,
		gpudevice
	);

	copyBuffer(stagingBuffer, indexbuffer, indexbuffersize);

	gpudevice.destroyBuffer(stagingBuffer,nullptr);
	gpudevice.freeMemory(stagingBufferMemory,nullptr);
}


void VEngine::createDescriptorSetLayout() {
	vk::DescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	vk::DescriptorSetLayoutCreateInfo createinfo{};
	createinfo.bindingCount = 1;
	createinfo.pBindings = &uboLayoutBinding;

	descriptorSetLayout = gpudevice.createDescriptorSetLayout(createinfo);


}


void VEngine::createGraphicspipeline() {

	auto vertexfile = readFile("shaders/vert.spv");
	auto fragfile   = readFile("shaders/frag.spv");

	//std::cout << vertexfile << std::endl;

	vk::ShaderModule vertexshader = setupShader(vertexfile,gpudevice);
	vk::ShaderModule fragshader   = setupShader(fragfile  ,gpudevice);

	vk::PipelineShaderStageCreateInfo vertStageInfo{};
	vertStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	vertStageInfo.module = vertexshader;
	vertStageInfo.pName = "main";

	vk::PipelineShaderStageCreateInfo fragStageInfo{};
	fragStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	fragStageInfo.module = fragshader;
	fragStageInfo.pName = "main";

/*
	vk::PipelineShaderStageCreateInfo fragStageInfo(
		{},
		vk::PipelineShaderStageCreateFlagBits::eAllowVaryingSubgroupSizeEXT,
		vk::ShaderStageFlagBits::eFragment,
		fragshader,
		"main"
	);*/

	vk::PipelineShaderStageCreateInfo pipelineStages[2] = {vertStageInfo,fragStageInfo};

	std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,vk::DynamicState::eScissor};

	vk::PipelineDynamicStateCreateInfo dynamicStateInfo(
		{},
		//{}, //no flags

		static_cast<uint32_t>(dynamicStates.size()),
		dynamicStates.data()
	);

	auto binddesc = Vertex::getbinddesc();
	auto attdesc = Vertex::getattributedesc();

	vk::PipelineVertexInputStateCreateInfo vertexInputstateInfo(
		{},
		//{}, //no flags

		1,
		&binddesc,

		static_cast<uint32_t>(attdesc.size()),
		attdesc.data() //already is an array of pointers

	);

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
		{},
		//{},// no flags

		vk::PrimitiveTopology::eTriangleList,
		false
	);

	vk::PipelineViewportStateCreateInfo viewportState{};
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	vk::PipelineRasterizationStateCreateInfo rasterizer(
		{},

		//{} // no flags
		false, //clip near and far clip planes
		false, //enable geometry being put through rasteriser

		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack,
		vk::FrontFace::eClockwise,

		false,//depth buffer bias
		0.0f,
		0.0f,
		0.0f,

		1.0f // line width
	);

	vk::PipelineMultisampleStateCreateInfo multisampling(
		{},
		//no flags
		vk::SampleCountFlagBits::e1,
		false,
		1.0f,
		nullptr,
		false,
		false
	);

	vk::PipelineColorBlendAttachmentState colorblendattach(
		false,
		
		vk::BlendFactor::eSrcAlpha,
		vk::BlendFactor::eOneMinusSrcAlpha,

		vk::BlendOp::eAdd,

		vk::BlendFactor::eOne,
		vk::BlendFactor::eZero,

		vk::BlendOp::eAdd,

		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
	);

	vk::PipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.logicOpEnable = false;
	colorBlending.logicOp = vk::LogicOp::eCopy;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorblendattach;
	//colorBlending.blendConstants 0.0f,0.0f,0.0f,0.0f


	vk::PipelineLayoutCreateInfo pipelineCreateinfo{};
	pipelineCreateinfo.setLayoutCount = 1;
	pipelineCreateinfo.pSetLayouts = &descriptorSetLayout;

	pipelineLayout = gpudevice.createPipelineLayout(pipelineCreateinfo);

	std::cout << "created pipeline layout" << std::endl;

	vk::GraphicsPipelineCreateInfo gpipeinfo(
		{},
		2,
		pipelineStages,
		&vertexInputstateInfo,
		&inputAssembly,
		nullptr,
		&viewportState,
		&rasterizer,
		&multisampling,
		nullptr,
		&colorBlending,
		&dynamicStateInfo,
		pipelineLayout,
		renderpass,
		0,
		VK_NULL_HANDLE,
		-1
	);


	vk::Result result;
	std::tie(result,graphicsPipeline) = gpudevice.createGraphicsPipeline(nullptr, gpipeinfo);//gpudevice.createGraphicsPipelines(VK_NULL_HANDLE,1,gpipeinfo,nullptr);


	gpudevice.destroyShaderModule(fragshader);
	gpudevice.destroyShaderModule(vertexshader);

}

void VEngine::createCommandPoolBuffer() {
	//create command pool + buffer

	//pool


	vk::CommandPoolCreateInfo cmdpoolinfo(
	
		vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		deviceQueueFams.graphicsFamily.value()
	);

	cmdPool = gpudevice.createCommandPool(cmdpoolinfo);
	std::cout << "created command pool" << std::endl;


	//buffer
	vk::CommandBufferAllocateInfo bufferinfo(
		
		cmdPool,
		vk::CommandBufferLevel::ePrimary,
		MAX_FRAMES_IN_FLIGHT
	);

	cmdBuffer = gpudevice.allocateCommandBuffers(bufferinfo); //multiple command buffers for multiple render frames at once
	//so u need to get the first element of the array
	std::cout << "command buffer allocated" << std::endl;

}

void VEngine::recreateSwapchainShit() {

	int w = 0,h = 0;

	glfwGetFramebufferSize(mainwindow,&w,&h);
	while (w == 0 || h == 0) {
		glfwGetFramebufferSize(mainwindow,&w,&h);
		glfwWaitEvents();
	}

	gpudevice.waitIdle();

	//remove old swapchain stuff
	clearSwapchain();

	createSwapchain(true);
	createImageviews();
	createFramebuffer();

}

void VEngine::setupVulkan() {
	
	if (enableValidationLayers == false) {
		throw std::runtime_error("validation layers are not supported.");
	}

	//creating instance
	std::vector<const char*> glfwextensions = getRequiredExtensions();


	vk::ApplicationInfo appinfo(
		"C++ Engine",
		1,
		"SEngine first",
		1,
		VK_API_VERSION_1_0
	);

	vk::InstanceCreateInfo createinfo({},
		&appinfo,
		valLayers.size(),  //validation layers
		valLayers.data(),
		glfwextensions.size(),  //extentions
		glfwextensions.data()
	);

	vulkaninstance = vk::createInstance(createinfo);
	//

	//debug messsenger
	setupDebugMessenger(vulkaninstance,debugmessenger,instanceLoader);

	if (debugmessenger) {
		std::cout << "got debug messenger" << std::endl;
	}
	//


	//surface
	vkdo(
		glfwCreateWindowSurface(vulkaninstance, mainwindow, nullptr, &csurface),
		"The window surface failed to be created"
	);

	surface = vk::UniqueSurfaceKHR(csurface, vulkaninstance);
	//

	//get physical device
	std::vector<vk::PhysicalDevice> devices = vulkaninstance.enumeratePhysicalDevices();
	for (auto& device : devices) {
		//get queue family information
		std::cout << device.getProperties().deviceName << std::endl;
		
		QueueFamilyIndices indices = getQueueFamsFromDevice(&device, &surface);
		bool supportsExtentions = checkDeviceSupportsExtentions(&device);
		SwapChainSupportDetails swpdetails = getSwapchainDetails(&device, &surface);

		if (indices.isComplete() && supportsExtentions && swpdetails.supported) {
			std::cout << "chosen" << std::endl;

			std::cout << "swpdetails " << swpdetails.supported << std::endl;

			gpu = device;
			deviceSwapchainDetails = swpdetails;
			deviceQueueFams = indices;
			break;
		}

	}

	//create logical device
	std::vector<vk::DeviceQueueCreateInfo> allQueueInfos;

	std::set<uint32_t> allQueueFamilies = {deviceQueueFams.graphicsFamily.value(),deviceQueueFams.presentFamily.value()};
	
	float priority = 1.0f;
	for (uint32_t family : allQueueFamilies) {
		vk::DeviceQueueCreateInfo createinfo(
			vk::DeviceQueueCreateFlags(),
			family,
			1,
			&priority
		);

		allQueueInfos.push_back(createinfo);
	}

	std::vector<const char*> desiredvallayers;
	if (enableValidationLayers) desiredvallayers = valLayers;
		
	vk::DeviceCreateInfo deviceinfo(

		vk::DeviceCreateFlags(),
		static_cast<uint32_t>(allQueueInfos.size()),
		allQueueInfos.data(),

		static_cast<uint32_t>(desiredvallayers.size()),
		desiredvallayers.data(),

		static_cast<uint32_t>(deviceExtensions.size()),
		deviceExtensions.data()

	);

	gpudevice = gpu.createDevice(deviceinfo);
	graphicsQueue = gpudevice.getQueue(deviceQueueFams.graphicsFamily.value(), 0);
	presentQueue = gpudevice.getQueue(deviceQueueFams.presentFamily.value(), 0);

	std::cout << "created gpu logical device" << std::endl;

	createSwapchain(false);
	createImageviews();

	createRenderpass();
	createDescriptorSetLayout();
	createGraphicspipeline();

	createFramebuffer();
	createCommandPoolBuffer();

	//sync objex

	vk::SemaphoreCreateInfo seminfo{};
	vk::FenceCreateInfo fenceinfo(vk::FenceCreateFlagBits::eSignaled);

	imageAvailableSem.resize(MAX_FRAMES_IN_FLIGHT);
	renderDoneSem.resize(MAX_FRAMES_IN_FLIGHT);
	frameFlightFen.resize(MAX_FRAMES_IN_FLIGHT);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		imageAvailableSem[i] = gpudevice.createSemaphore(seminfo);
		renderDoneSem[i] = gpudevice.createSemaphore(seminfo);
   		frameFlightFen[i] = gpudevice.createFence(fenceinfo);
	}

	//imageAvailableSem = gpudevice.createSemaphore(seminfo);
	//renderDoneSem = gpudevice.createSemaphore(seminfo);
   // frameFlightFen = gpudevice.createFence(fenceinfo);

    std::cout << "created semaphores and fences" << std::endl;

    createVertBuffer();
    createIndexBuffer();

    std::cout << "created vertex buffer" << std::endl;

    //all done!
    std::cout << "setup all done!!!" << std::endl;



}	

void VEngine::bufferRecord(uint32_t index) {

	vk::CommandBufferBeginInfo begininfo{};
	cmdBuffer[currentFrame].begin(begininfo);
	//record began

	vk::Rect2D rect;
	rect.offset.x = 0;
	rect.offset.y = 0;
	rect.extent = swapChainExtent;

	vk::ClearValue clearcol{};
	clearcol.color = vk::ClearColorValue(std::array<float, 4>({{0.1f, 0.1f, 0.2f, 1.0f}}));

	vk::RenderPassBeginInfo renderpassinfo( 
		//{},
		renderpass,
		swapChainFramebuffer[index],
		rect,
		1,
		&clearcol
	);

	cmdBuffer[currentFrame].beginRenderPass(renderpassinfo,vk::SubpassContents::eInline);
	cmdBuffer[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

	vk::Buffer vertbuffers[] = {vertexbuffer};
	vk::DeviceSize offsets[] = {0};

	cmdBuffer[currentFrame].bindVertexBuffers(0, 1, vertbuffers, offsets);
	cmdBuffer[currentFrame].bindIndexBuffer(indexbuffer, 0, vk::IndexType::eUint32);

	vk::Viewport vp;
	vp.x = 0.0f;
	vp.y = 0.0f;
	vp.width = (float) swapChainExtent.width;
	vp.height = (float) swapChainExtent.height;
	vp.maxDepth = 1.0f;
	cmdBuffer[currentFrame].setViewport(0, 1, &vp);

	vk::Rect2D scissorrect = rect;
	//scissorrect.offset = {0,0};
	//scissorrect.extent = swapChainExtent;
	cmdBuffer[currentFrame].setScissor(0, 1, &scissorrect);
	//cmdBuffer[currentFrame].draw(static_cast<uint32_t>(vertices.size()), 1, 0, 0);
	cmdBuffer[currentFrame].drawIndexed(static_cast<uint32_t>(vertex_indices.size()) ,1 ,0 ,0 ,0);

	cmdBuffer[currentFrame].endRenderPass();
	cmdBuffer[currentFrame].end();
	

}



void VEngine::draw(){
	//everything here fricking returns a result
	[[maybe_unused]] vk::Result res;

	res = gpudevice.waitForFences(1,&frameFlightFen[currentFrame],true,UINT64_MAX);

	uint32_t nextimageindex = 0;

	res = gpudevice.acquireNextImageKHR(
		swapChain, 
		UINT64_MAX,
		imageAvailableSem[currentFrame],
		VK_NULL_HANDLE,
		&nextimageindex
	);

	if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR || windowCurrentlyResized == true) {
		windowCurrentlyResized = false;
		recreateSwapchainShit();
		return;
	} else if (res != vk::Result::eSuccess) {
		throw std::runtime_error("failed to get a good swapchain image my man");
	}

	res = gpudevice.resetFences(1, &frameFlightFen[currentFrame]);

	cmdBuffer[currentFrame].reset();
	bufferRecord(nextimageindex);

	vk::Semaphore wsems[] = {imageAvailableSem[currentFrame]};
	vk::Semaphore signalsems[] = {renderDoneSem[currentFrame]};
	vk::PipelineStageFlags wstages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

	vk::SubmitInfo sinfo(
		//{},

		1,
		wsems,
		wstages,

		1,
		&cmdBuffer[currentFrame],

		1,
		signalsems
	);

	res = graphicsQueue.submit(1, &sinfo, frameFlightFen[currentFrame]);

    vk::PresentInfoKHR presentInfo(
	//	{},
		1,
		signalsems,

		1,
		&swapChain,

		&nextimageindex
	);

	res = presentQueue.presentKHR(&presentInfo);

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

}

void VEngine::engineloop() {

	std::cout << "starting main loop" << std::endl;

	while (!glfwWindowShouldClose(mainwindow)) {
		glfwPollEvents();
		draw();
	}

	gpudevice.waitIdle();

}
