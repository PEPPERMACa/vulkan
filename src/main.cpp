#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;
constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef __APPLE__
    "VK_KHR_portability_subset",
#endif
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct DeviceContext {
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct SwapChainContext {
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
};

struct GraphicsPipelineContext {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

struct CommandContext {
    VkCommandPool pool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> buffers;
};

struct SyncContext {
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    VkFence inFlightFence = VK_NULL_HANDLE;
};

void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto* framebufferResized = static_cast<bool*>(glfwGetWindowUserPointer(window));
    if (framebufferResized != nullptr) {
        *framebufferResized = true;
    }

    std::cout << "Framebuffer resized: " << width << "x" << height << '\n';
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*
) {
    const char* severity = "INFO";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severity = "ERROR";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severity = "WARNING";
    }

    std::cerr << "[Vulkan " << severity << "] " << callbackData->pMessage << '\n';
    return VK_FALSE;
}

std::vector<const char*> getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    if (glfwExtensions == nullptr) {
        throw std::runtime_error("GLFW could not find Vulkan instance extensions.");
    }

    std::vector<const char*> extensions(
        glfwExtensions,
        glfwExtensions + glfwExtensionCount
    );

#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

    return extensions;
}

bool isValidationLayerAvailable(const char* layerName) {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const auto& layer : availableLayers) {
        if (std::strcmp(layer.layerName, layerName) == 0) {
            return true;
        }
    }

    return false;
}

bool shouldEnableValidationLayer() {
    return isValidationLayerAvailable(kValidationLayer);
}

void populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo
) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

VkResult createDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    const VkAllocationCallbacks* allocator,
    VkDebugUtilsMessengerEXT* debugMessenger
) {
    auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
    );

    if (function == nullptr) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    return function(instance, createInfo, allocator, debugMessenger);
}

void destroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* allocator
) {
    auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
    );

    if (function != nullptr) {
        function(instance, debugMessenger, allocator);
    }
}

VkInstance createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Lesson 14";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    const bool enableValidationLayer = shouldEnableValidationLayer();
    auto extensions = getRequiredExtensions();
    if (enableValidationLayer) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef __APPLE__
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    std::vector<const char*> validationLayers;
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayer) {
        validationLayers.push_back(kValidationLayer);
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance.");
    }

    std::cout << "Created Vulkan instance.\n";
    std::cout << "Enabled extensions:\n";
    for (const char* extension : extensions) {
        std::cout << "  " << extension << '\n';
    }

    if (!validationLayers.empty()) {
        std::cout << "Enabled validation layer: " << kValidationLayer << '\n';
    } else {
        std::cout << "Validation layer not found; continuing without it.\n";
    }

    return instance;
}

VkDebugUtilsMessengerEXT createDebugMessenger(VkInstance instance) {
    if (!shouldEnableValidationLayer()) {
        return VK_NULL_HANDLE;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    if (createDebugUtilsMessengerEXT(
            instance,
            &createInfo,
            nullptr,
            &debugMessenger
        ) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger.");
    }

    std::cout << "Created debug messenger.\n";
    return debugMessenger;
}

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan window surface.");
    }

    std::cout << "Created Vulkan window surface.\n";
    return surface;
}

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        device,
        &queueFamilyCount,
        queueFamilies.data()
    );

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(
        device,
        nullptr,
        &extensionCount,
        availableExtensions.data()
    );

    std::set<std::string> requiredExtensions(
        kDeviceExtensions.begin(),
        kDeviceExtensions.end()
    );

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

SwapChainSupportDetails querySwapChainSupport(
    VkPhysicalDevice device,
    VkSurfaceKHR surface
) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device,
        surface,
        &details.capabilities
    );

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device,
            surface,
            &formatCount,
            details.formats.data()
        );
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device,
        surface,
        &presentModeCount,
        nullptr
    );
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device,
            surface,
            &presentModeCount,
            details.presentModes.data()
        );
    }

    return details;
}

bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices = findQueueFamilies(device, surface);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
        swapChainAdequate =
            !swapChainSupport.formats.empty() &&
            !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable GPU found.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);

        std::cout << "Checking physical device: " << properties.deviceName << '\n';

        if (isDeviceSuitable(device, surface)) {
            QueueFamilyIndices indices = findQueueFamilies(device, surface);
            std::cout << "Selected physical device: " << properties.deviceName << '\n';
            std::cout << "Graphics queue family: " << indices.graphicsFamily.value() << '\n';
            std::cout << "Present queue family: " << indices.presentFamily.value() << '\n';

            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
            std::cout << "Swap chain formats: " << swapChainSupport.formats.size() << '\n';
            std::cout << "Swap chain present modes: "
                      << swapChainSupport.presentModes.size() << '\n';
            return device;
        }
    }

    throw std::runtime_error("Failed to find a suitable GPU.");
}

DeviceContext createLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value(),
    };

    float queuePriority = 1.0F;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    DeviceContext context{};
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &context.device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device.");
    }

    vkGetDeviceQueue(context.device, indices.graphicsFamily.value(), 0, &context.graphicsQueue);
    vkGetDeviceQueue(context.device, indices.presentFamily.value(), 0, &context.presentQueue);

    std::cout << "Created logical device.\n";
    std::cout << "Retrieved graphics queue.\n";
    std::cout << "Retrieved present queue.\n";

    return context;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats
) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes
) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    GLFWwindow* window
) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D actualExtent{
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
    };

    actualExtent.width = std::max(
        capabilities.minImageExtent.width,
        std::min(capabilities.maxImageExtent.width, actualExtent.width)
    );
    actualExtent.height = std::max(
        capabilities.minImageExtent.height,
        std::min(capabilities.maxImageExtent.height, actualExtent.height)
    );

    return actualExtent;
}

SwapChainContext createSwapChain(
    VkPhysicalDevice physicalDevice,
    const DeviceContext& deviceContext,
    VkSurfaceKHR surface,
    GLFWwindow* window
) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(
        physicalDevice,
        surface
    );

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(
        swapChainSupport.formats
    );
    VkPresentModeKHR presentMode = chooseSwapPresentMode(
        swapChainSupport.presentModes
    );
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, window);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
    uint32_t queueFamilyIndices[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value(),
    };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    SwapChainContext context{};
    if (vkCreateSwapchainKHR(
            deviceContext.device,
            &createInfo,
            nullptr,
            &context.swapChain
        ) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain.");
    }

    vkGetSwapchainImagesKHR(deviceContext.device, context.swapChain, &imageCount, nullptr);
    context.images.resize(imageCount);
    vkGetSwapchainImagesKHR(
        deviceContext.device,
        context.swapChain,
        &imageCount,
        context.images.data()
    );

    context.imageFormat = surfaceFormat.format;
    context.extent = extent;

    std::cout << "Created swap chain.\n";
    std::cout << "Swap chain images: " << context.images.size() << '\n';
    std::cout << "Swap chain extent: " << context.extent.width
              << "x" << context.extent.height << '\n';

    return context;
}

VkImageView createImageView(
    VkDevice device,
    VkImage image,
    VkFormat format
) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &createInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view.");
    }

    return imageView;
}

void createImageViews(VkDevice device, SwapChainContext& swapChainContext) {
    swapChainContext.imageViews.resize(swapChainContext.images.size());

    for (size_t i = 0; i < swapChainContext.images.size(); ++i) {
        swapChainContext.imageViews[i] = createImageView(
            device,
            swapChainContext.images[i],
            swapChainContext.imageFormat
        );
    }

    std::cout << "Created swap chain image views: "
              << swapChainContext.imageViews.size() << '\n';
}

VkRenderPass createRenderPass(VkDevice device, VkFormat swapChainImageFormat) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentReference{};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }

    std::cout << "Created render pass.\n";
    return renderPass;
}

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filename);
    }

    const std::streamsize fileSize = file.tellg();
    std::vector<char> buffer(static_cast<size_t>(fileSize));

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

VkShaderModule createShaderModule(
    VkDevice device,
    const std::vector<char>& shaderCode
) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module.");
    }

    return shaderModule;
}

GraphicsPipelineContext createGraphicsPipeline(
    VkDevice device,
    VkExtent2D swapChainExtent,
    VkRenderPass renderPass
) {
    const auto vertexShaderCode = readFile(
        std::string(SHADER_DIR) + "/triangle.vert.spv"
    );
    const auto fragmentShaderCode = readFile(
        std::string(SHADER_DIR) + "/triangle.frag.spv"
    );

    VkShaderModule vertexShaderModule = createShaderModule(device, vertexShaderCode);
    VkShaderModule fragmentShaderModule = createShaderModule(device, fragmentShaderCode);

    VkPipelineShaderStageCreateInfo vertexShaderStageInfo{};
    vertexShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageInfo.module = vertexShaderModule;
    vertexShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentShaderStageInfo{};
    fragmentShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageInfo.module = fragmentShaderModule;
    fragmentShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertexShaderStageInfo,
        fragmentShaderStageInfo,
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0F;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    GraphicsPipelineContext context{};
    if (vkCreatePipelineLayout(
            device,
            &pipelineLayoutInfo,
            nullptr,
            &context.layout
        ) != VK_SUCCESS) {
        vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
        vkDestroyShaderModule(device, vertexShaderModule, nullptr);
        throw std::runtime_error("Failed to create pipeline layout.");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = context.layout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(
            device,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &context.pipeline
        ) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, context.layout, nullptr);
        vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
        vkDestroyShaderModule(device, vertexShaderModule, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline.");
    }

    vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
    vkDestroyShaderModule(device, vertexShaderModule, nullptr);

    std::cout << "Created graphics pipeline.\n";
    return context;
}

void createFramebuffers(
    VkDevice device,
    SwapChainContext& swapChainContext,
    VkRenderPass renderPass
) {
    swapChainContext.framebuffers.resize(swapChainContext.imageViews.size());

    for (size_t i = 0; i < swapChainContext.imageViews.size(); ++i) {
        VkImageView attachments[] = {
            swapChainContext.imageViews[i],
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainContext.extent.width;
        framebufferInfo.height = swapChainContext.extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(
                device,
                &framebufferInfo,
                nullptr,
                &swapChainContext.framebuffers[i]
            ) != VK_SUCCESS) {
            for (VkFramebuffer framebuffer : swapChainContext.framebuffers) {
                if (framebuffer != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(device, framebuffer, nullptr);
                }
            }
            throw std::runtime_error("Failed to create framebuffer.");
        }
    }

    std::cout << "Created swap chain framebuffers: "
              << swapChainContext.framebuffers.size() << '\n';
}

CommandContext createCommandContext(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    const SwapChainContext& swapChainContext,
    VkRenderPass renderPass,
    const GraphicsPipelineContext& graphicsPipeline
) {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(
        physicalDevice,
        surface
    );

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    CommandContext context{};
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &context.pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool.");
    }

    context.buffers.resize(swapChainContext.framebuffers.size());

    /* Command buffer pool owns and allocates memory for command buffers */
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = context.pool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = static_cast<uint32_t>(context.buffers.size());

    if (vkAllocateCommandBuffers(
            device,
            &allocateInfo,
            context.buffers.data()
        ) != VK_SUCCESS) {
        vkDestroyCommandPool(device, context.pool, nullptr);
        throw std::runtime_error("Failed to allocate command buffers.");
    }

    for (size_t i = 0; i < context.buffers.size(); ++i) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(context.buffers[i], &beginInfo) != VK_SUCCESS) {
            vkDestroyCommandPool(device, context.pool, nullptr);
            throw std::runtime_error("Failed to begin recording command buffer.");
        }

        VkClearValue clearColor{};
        clearColor.color = {{0.0F, 0.0F, 0.0F, 1.0F}};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainContext.framebuffers[i];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainContext.extent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(
            context.buffers[i],
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE
        );
        vkCmdBindPipeline(
            context.buffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphicsPipeline.pipeline
        );
        vkCmdDraw(context.buffers[i], 3, 1, 0, 0);
        vkCmdEndRenderPass(context.buffers[i]);

        if (vkEndCommandBuffer(context.buffers[i]) != VK_SUCCESS) {
            vkDestroyCommandPool(device, context.pool, nullptr);
            throw std::runtime_error("Failed to record command buffer.");
        }
    }

    std::cout << "Recorded command buffers: " << context.buffers.size() << '\n';
    return context;
}

SyncContext createSyncContext(VkDevice device, size_t swapChainImageCount) {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    SyncContext context{};
    context.renderFinishedSemaphores.resize(swapChainImageCount);

    if (vkCreateSemaphore(
            device,
            &semaphoreInfo,
            nullptr,
            &context.imageAvailableSemaphore
        ) != VK_SUCCESS ||
        vkCreateFence(
            device,
            &fenceInfo,
            nullptr,
            &context.inFlightFence
        ) != VK_SUCCESS) {
        if (context.inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, context.inFlightFence, nullptr);
        }
        if (context.imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, context.imageAvailableSemaphore, nullptr);
        }
        throw std::runtime_error("Failed to create synchronization objects.");
    }

    for (size_t i = 0; i < context.renderFinishedSemaphores.size(); ++i) {
        if (vkCreateSemaphore(
                device,
                &semaphoreInfo,
                nullptr,
                &context.renderFinishedSemaphores[i]
            ) != VK_SUCCESS) {
            for (VkSemaphore semaphore : context.renderFinishedSemaphores) {
                if (semaphore != VK_NULL_HANDLE) {
                    vkDestroySemaphore(device, semaphore, nullptr);
                }
            }
            vkDestroyFence(device, context.inFlightFence, nullptr);
            vkDestroySemaphore(device, context.imageAvailableSemaphore, nullptr);
            throw std::runtime_error("Failed to create synchronization objects.");
        }
    }

    std::cout << "Created synchronization objects.\n";
    return context;
}

void destroySyncContext(VkDevice device, const SyncContext& syncContext) {
    if (syncContext.inFlightFence != VK_NULL_HANDLE) {
        vkDestroyFence(device, syncContext.inFlightFence, nullptr);
    }

    for (VkSemaphore semaphore : syncContext.renderFinishedSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
    }

    if (syncContext.imageAvailableSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, syncContext.imageAvailableSemaphore, nullptr);
    }
}

void destroySwapChainResources(
    VkDevice device,
    SwapChainContext& swapChainContext,
    VkRenderPass& renderPass,
    GraphicsPipelineContext& graphicsPipeline,
    CommandContext& commandContext
) {
    if (commandContext.pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandContext.pool, nullptr);
        commandContext.pool = VK_NULL_HANDLE;
        commandContext.buffers.clear();
    }

    for (VkFramebuffer framebuffer : swapChainContext.framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    swapChainContext.framebuffers.clear();

    if (graphicsPipeline.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipeline.pipeline, nullptr);
        graphicsPipeline.pipeline = VK_NULL_HANDLE;
    }
    if (graphicsPipeline.layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, graphicsPipeline.layout, nullptr);
        graphicsPipeline.layout = VK_NULL_HANDLE;
    }

    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }

    for (VkImageView imageView : swapChainContext.imageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    swapChainContext.imageViews.clear();

    if (swapChainContext.swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapChainContext.swapChain, nullptr);
        swapChainContext.swapChain = VK_NULL_HANDLE;
    }
    swapChainContext.images.clear();
}

void rebuildSwapChainResources(
    VkPhysicalDevice physicalDevice,
    const DeviceContext& deviceContext,
    VkSurfaceKHR surface,
    GLFWwindow* window,
    SwapChainContext& swapChainContext,
    VkRenderPass& renderPass,
    GraphicsPipelineContext& graphicsPipeline,
    CommandContext& commandContext,
    SyncContext& syncContext
) {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwWaitEvents();
        glfwGetFramebufferSize(window, &width, &height);
    }

    std::cout << "Rebuilding swap chain for framebuffer size: "
              << width << "x" << height << '\n';

    vkDeviceWaitIdle(deviceContext.device);
    destroySyncContext(deviceContext.device, syncContext);
    destroySwapChainResources(
        deviceContext.device,
        swapChainContext,
        renderPass,
        graphicsPipeline,
        commandContext
    );

    swapChainContext = createSwapChain(
        physicalDevice,
        deviceContext,
        surface,
        window
    );
    createImageViews(deviceContext.device, swapChainContext);
    renderPass = createRenderPass(
        deviceContext.device,
        swapChainContext.imageFormat
    );
    graphicsPipeline = createGraphicsPipeline(
        deviceContext.device,
        swapChainContext.extent,
        renderPass
    );
    createFramebuffers(
        deviceContext.device,
        swapChainContext,
        renderPass
    );
    commandContext = createCommandContext(
        deviceContext.device,
        physicalDevice,
        surface,
        swapChainContext,
        renderPass,
        graphicsPipeline
    );
    syncContext = createSyncContext(
        deviceContext.device,
        swapChainContext.images.size()
    );

    std::cout << "Finished swap chain rebuild.\n";
}

bool drawFrame(
    const DeviceContext& deviceContext,
    const SwapChainContext& swapChainContext,
    const CommandContext& commandContext,
    const SyncContext& syncContext,
    bool& framebufferResized
) {
    vkWaitForFences(
        deviceContext.device,
        1,
        &syncContext.inFlightFence,
        VK_TRUE,
        std::numeric_limits<uint64_t>::max()
    );

    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        deviceContext.device,
        swapChainContext.swapChain,
        std::numeric_limits<uint64_t>::max(),
        syncContext.imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        return true;
    }

    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image.");
    }

    vkResetFences(deviceContext.device, 1, &syncContext.inFlightFence);

    VkSemaphore waitSemaphores[] = {
        syncContext.imageAvailableSemaphore,
    };
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    VkSemaphore signalSemaphores[] = {
        syncContext.renderFinishedSemaphores[imageIndex],
    };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandContext.buffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(
            deviceContext.graphicsQueue,
            1,
            &submitInfo,
            syncContext.inFlightFence
        ) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer.");
    }

    VkSwapchainKHR swapChains[] = {
        swapChainContext.swapChain,
    };

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(
        deviceContext.presentQueue,
        &presentInfo
    );

    if (
        presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
        presentResult == VK_SUBOPTIMAL_KHR ||
        framebufferResized
    ) {
        framebufferResized = false;
        return true;
    }

    if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to present swap chain image.");
    }

    return false;
}

} // namespace

int main() {
    try {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW.");
        }

        if (!glfwVulkanSupported()) {
            throw std::runtime_error("GLFW says Vulkan is not supported.");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        GLFWwindow* window = glfwCreateWindow(
            kWindowWidth,
            kWindowHeight,
            "Vulkan Lesson 14",
            nullptr,
            nullptr
        );

        if (window == nullptr) {
            throw std::runtime_error("Failed to create GLFW window.");
        }

        bool framebufferResized = false;
        glfwSetWindowUserPointer(window, &framebufferResized);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

        VkInstance instance = createInstance();
        VkDebugUtilsMessengerEXT debugMessenger = createDebugMessenger(instance);
        VkSurfaceKHR surface = createSurface(instance, window);
        VkPhysicalDevice physicalDevice = pickPhysicalDevice(instance, surface);
        DeviceContext deviceContext = createLogicalDevice(physicalDevice, surface);
        SwapChainContext swapChainContext = createSwapChain(
            physicalDevice,
            deviceContext,
            surface,
            window
        );
        createImageViews(deviceContext.device, swapChainContext);
        VkRenderPass renderPass = createRenderPass(
            deviceContext.device,
            swapChainContext.imageFormat
        );
        GraphicsPipelineContext graphicsPipeline = createGraphicsPipeline(
            deviceContext.device,
            swapChainContext.extent,
            renderPass
        );
        createFramebuffers(
            deviceContext.device,
            swapChainContext,
            renderPass
        );
        CommandContext commandContext = createCommandContext(
            deviceContext.device,
            physicalDevice,
            surface,
            swapChainContext,
            renderPass,
            graphicsPipeline
        );
        SyncContext syncContext = createSyncContext(
            deviceContext.device,
            swapChainContext.images.size()
        );

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            const bool shouldRebuildSwapChain = drawFrame(
                deviceContext,
                swapChainContext,
                commandContext,
                syncContext,
                framebufferResized
            );
            if (shouldRebuildSwapChain) {
                rebuildSwapChainResources(
                    physicalDevice,
                    deviceContext,
                    surface,
                    window,
                    swapChainContext,
                    renderPass,
                    graphicsPipeline,
                    commandContext,
                    syncContext
                );
            }
        }

        vkDeviceWaitIdle(deviceContext.device);
        destroySyncContext(deviceContext.device, syncContext);
        destroySwapChainResources(
            deviceContext.device,
            swapChainContext,
            renderPass,
            graphicsPipeline,
            commandContext
        );
        vkDestroyDevice(deviceContext.device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        if (debugMessenger != VK_NULL_HANDLE) {
            destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        glfwTerminate();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
