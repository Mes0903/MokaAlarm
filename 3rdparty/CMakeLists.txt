# 3rdparty/CMakeLists.txt

# ─── ImGui (Win32 + Vulkan backends) ─────────────────────────────────────────
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMGUI_BACKENDS_DIR ${IMGUI_DIR}/backends)

add_library(IMGUI_LIB STATIC
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
	${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_BACKENDS_DIR}/imgui_impl_win32.cpp
	${IMGUI_BACKENDS_DIR}/imgui_impl_vulkan.cpp
)

target_include_directories(IMGUI_LIB PUBLIC
	${IMGUI_DIR}
	${IMGUI_BACKENDS_DIR}
	${Vulkan_INCLUDE_DIRS}
)

target_compile_definitions(IMGUI_LIB PUBLIC VK_USE_PLATFORM_WIN32_KHR)
target_link_libraries(IMGUI_LIB PUBLIC ${Vulkan_LIBRARIES})
