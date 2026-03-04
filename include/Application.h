#pragma once

// windows.h must come first so Win32 types (HWND, LRESULT, etc.) are defined
// before shellapi.h and imgui_impl_win32.h use them.
#include <windows.h>
#include <shellapi.h>

// VK_USE_PLATFORM_WIN32_KHR is set via compile definition in CMakeLists.txt.
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_win32.h"
#include <memory>
#include <vulkan/vulkan.h>

namespace alarm::controller {
class AlarmController;
}
namespace alarm::view {
class AlarmView;
}

#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
#endif

class Application {
public:
	Application();
	~Application(); // both defined in .cpp where AlarmController/AlarmView are complete

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
	void setWindowVisible_(bool v) noexcept;

	// Vulkan helpers
	static bool isExtensionAvailable_(const ImVector<VkExtensionProperties> &props, const char *name) noexcept;
	static void checkVkResult_(VkResult err) noexcept;

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

	// ── Alarm subsystem ────────────────────────────────────────────────────
	std::unique_ptr<alarm::controller::AlarmController> ctrl_;
	std::unique_ptr<alarm::view::AlarmView> view_;
};
