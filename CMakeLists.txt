cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(VulkanKHRRaytracer)

find_package(Vulkan REQUIRED FATAL_ERROR)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Setup Vulkan-Headers
set(VK_HEADERS_INCLUDE "${PROJECT_SOURCE_DIR}/third_party/Vulkan-Headers/include")

add_library(Vulkan-Headers INTERFACE)
target_include_directories(Vulkan-Headers INTERFACE ${VK_HEADERS_INCLUDE})

# Setup VOLK
set(VOLK_ROOT "${PROJECT_SOURCE_DIR}/third_party/volk")

add_library(volk STATIC ${VOLK_ROOT}/volk.h ${VOLK_ROOT}/volk.c)
target_include_directories(volk PUBLIC ${VOLK_ROOT}/)

target_link_libraries(volk Vulkan-Headers)

# Setup GLM
set(GLM_ROOT "${PROJECT_SOURCE_DIR}/third_party/glm")

add_library(glm INTERFACE)
target_include_directories(glm INTERFACE ${GLM_ROOT})

# Setup GLFW
set(GLFW_ROOT "${PROJECT_SOURCE_DIR}/third_party/glfw")

if(WIN32)
	set(GLFW_PLATFORM_SOURCES ${GLFW_ROOT}/src/win32_init.c
							  ${GLFW_ROOT}/src/win32_joystick.c
							  ${GLFW_ROOT}/src/win32_monitor.c
							  ${GLFW_ROOT}/src/win32_time.c
							  ${GLFW_ROOT}/src/win32_thread.c
							  ${GLFW_ROOT}/src/win32_window.c
							  ${GLFW_ROOT}/src/wgl_context.c
							  ${GLFW_ROOT}/src/egl_context.c
							  ${GLFW_ROOT}/src/osmesa_context.c)
	
	add_compile_definitions(_GLFW_WIN32)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	add_compile_definitions(VK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
	set(GLFW_PLATFORM_SOURCES "")
elseif(UNIX)
	set(GLFW_PLATFORM_SOURCES ${GLFW_ROOT}/src/x11_init.c
							  ${GLFW_ROOT}/src/x11_monitor.c
							  ${GLFW_ROOT}/src/x11_window.c
							  ${GLFW_ROOT}/src/xkb_unicode.c
							  ${GLFW_ROOT}/src/posix_time.c
							  ${GLFW_ROOT}/src/posix_thread.c
							  ${GLFW_ROOT}/src/glx_context.c
							  ${GLFW_ROOT}/src/egl_context.c
							  ${GLFW_ROOT}/src/osmesa_context.c
							  ${GLFW_ROOT}/src/linux_joystick.c)
	
	add_compile_definitions(_GLFW_X11)
endif()

set(GLFW_SOURCES ${GLFW_ROOT}/src/glfw_config.h
				 ${GLFW_ROOT}/src/context.c
				 ${GLFW_ROOT}/src/init.c
				 ${GLFW_ROOT}/src/input.c
				 ${GLFW_ROOT}/src/monitor.c
				 ${GLFW_ROOT}/src/vulkan.c
				 ${GLFW_ROOT}/src/window.c
				 ${GLFW_PLATFORM_SOURCES})

add_library(glfw STATIC ${GLFW_SOURCES})
target_include_directories(glfw PUBLIC ${GLFW_ROOT}/include)

target_link_libraries(glfw Vulkan-Headers volk)

# Setup ASSIMP
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Turn off ASSIMP tests.")
set(ASSIMP_INSTALL OFF CACHE BOOL "Do not install ASSIMP")
set(ASSIMP_BUILD_DRACO OFF CACHE BOOL "Do not install ASSIMP")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Do not build ASSIMP tools")

add_subdirectory(third_party/assimp)

# Setup glslang
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "")
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "")
set(ENABLE_CTEST OFF CACHE BOOL "")
set(ENABLE_HLSL OFF CACHE BOOL "")

add_subdirectory(third_party/glslang)

# Setup STB
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${PROJECT_SOURCE_DIR}/third_party/stb/")

# Setup ImGUI
set(IMGUI_ROOT "${PROJECT_SOURCE_DIR}/third_party/imgui")

file(GLOB IMGUI_SOURCES "${IMGUI_ROOT}/*.cpp" "${IMGUI_ROOT}/*.h")

list(APPEND IMGUI_SOURCES 	"${IMGUI_ROOT}/backends/imgui_impl_glfw.cpp"
							"${IMGUI_ROOT}/backends/imgui_impl_glfw.h"
							"${IMGUI_ROOT}/backends/imgui_impl_vulkan.cpp"
							"${IMGUI_ROOT}/backends/imgui_impl_vulkan.h")

add_library(imgui ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC "${IMGUI_ROOT}/")

add_compile_definitions(imgui PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES)

target_link_libraries(imgui Vulkan-Headers volk glfw stb)

# Setup sources
configure_file(src/ProjectBase.h.in generated_headers/ProjectBase.h)

file(GLOB_RECURSE SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/*.h" "${PROJECT_SOURCE_DIR}/src/*.cpp")

include_directories("${PROJECT_SOURCE_DIR}/src/")

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} Vulkan-Headers)
target_link_libraries(${PROJECT_NAME} glfw)
target_link_libraries(${PROJECT_NAME} glm)
target_link_libraries(${PROJECT_NAME} volk)
target_link_libraries(${PROJECT_NAME} assimp::assimp)
target_link_libraries(${PROJECT_NAME} glslang)
target_link_libraries(${PROJECT_NAME} SPIRV)
target_link_libraries(${PROJECT_NAME} stb)
target_link_libraries(${PROJECT_NAME} imgui)

target_include_directories(${PROJECT_NAME} PUBLIC src build/generated_headers)

# Setup IDE filters
macro(ConvertToFilters curdir)
	file(GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${curdir} ${PROJECT_SOURCE_DIR}/${curdir}/*)
	foreach(child ${children})
		if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${curdir}/${child})
			ConvertToFilters(${curdir}/${child})
		else()
			string(REPLACE "/" "\\" groupname ${curdir})
			
			source_group(${groupname} FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
		endif()
	endforeach()
endmacro()

ConvertToFilters(data)
ConvertToFilters(src)

if(WIN32)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
					   COMMAND ${CMAKE_COMMAND} -E copy_directory
					   $<TARGET_FILE_DIR:assimp>
					   $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()