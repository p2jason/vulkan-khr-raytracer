cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(VulkanKHRRaytracer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Setup Vulkan-Headers
set(VK_HEADERS_INCLUDE "${PROJECT_SOURCE_DIR}/third_party/Vulkan-Headers/include")

add_library(Vulkan-Headers INTERFACE)
target_include_directories(Vulkan-Headers INTERFACE ${VK_HEADERS_INCLUDE})

# Setup VOLK
set(VOLK_ROOT "${PROJECT_SOURCE_DIR}/third_party/volk")

add_library(volk ${VOLK_ROOT}/volk.h ${VOLK_ROOT}/volk.c)
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

add_library(glfw ${GLFW_SOURCES})
target_include_directories(glfw PUBLIC ${GLFW_ROOT}/include)

target_link_libraries(glfw Vulkan-Headers)

# Setup sources
file(GLOB_RECURSE SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/*.h" "${PROJECT_SOURCE_DIR}/src/*.cpp")

include_directories("${PROJECT_SOURCE_DIR}/src/")

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} Vulkan-Headers)
target_link_libraries(${PROJECT_NAME} glfw)
target_link_libraries(${PROJECT_NAME} glm)
target_link_libraries(${PROJECT_NAME} volk)

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

ConvertToFilters(src)
ConvertToFilters(data)