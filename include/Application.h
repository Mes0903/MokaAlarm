#pragma once

// VK_USE_PLATFORM_WIN32_KHR is set via compile definition in CMakeLists.txt.
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_win32.h"
#include <shellapi.h>
#include <vulkan/vulkan.h>
#include <windows.h>

#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
#endif

class Application {
public:
	Application()	 = default;
	~Application() = default;

	int run();

	// Dispatched by the static WndProc.
	LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
	// Init / cleanup
	void initWindow_();
	void initVulkan_();
	void setupVulkanWindow_(VkSurfaceKHR surface, int w, int h);
	void initImGui_();
	void initTray_();
	void cleanup_();
	void cleanupVulkan_();
	void cleanupVulkanWindow_();

	// Main loop
	void mainLoop_();
	void frameRender_(ImDrawData *draw);
	void framePresent_();

	// Tray helpers
	void showTrayMenu_();
	void setWindowVisible_(bool v);

	// Vulkan helpers
	static bool isExtensionAvailable_(const ImVector<VkExtensionProperties> &props, const char *name);
	static void checkVkResult_(VkResult err);

	// ── Win32 ──────────────────────────────────────────────────────────────
	HWND hwnd_			= nullptr;
	WNDCLASSEXW wc_ = {};

	// ── Vulkan ─────────────────────────────────────────────────────────────
	VkAllocationCallbacks *allocator_ = nullptr;
	VkInstance instance_							= VK_NULL_HANDLE;
	VkPhysicalDevice physDev_					= VK_NULL_HANDLE;
	VkDevice device_									= VK_NULL_HANDLE;
	uint32_t queueFamily_							= static_cast<uint32_t>(-1);
	VkQueue queue_										= VK_NULL_HANDLE;
	VkPipelineCache pipelineCache_		= VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool_	= VK_NULL_HANDLE;
#ifdef APP_USE_VULKAN_DEBUG_REPORT
	VkDebugReportCallbackEXT debugReport_ = VK_NULL_HANDLE;
#endif

	ImGui_ImplVulkanH_Window wndData_ = {};
	uint32_t minImageCount_						= 2;
	bool swapChainRebuild_						= false;

	// ── Tray ───────────────────────────────────────────────────────────────
	NOTIFYICONDATAW nid_ = {};

	// ── State ──────────────────────────────────────────────────────────────
	bool running_				= true;
	bool windowVisible_ = true;
};
