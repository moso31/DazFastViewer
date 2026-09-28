cmake_minimum_required(VERSION 3.24)
project(DfvPhysicsProbe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(DFV_JOLT_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../.research/JoltPhysics-v5.6.0" CACHE PATH "Jolt v5.6.0 源码")
if(NOT EXISTS "${DFV_JOLT_SOURCE}/Build/CMakeLists.txt")
  message(FATAL_ERROR "请先按调研文档下载 Jolt v5.6.0")
endif()
# 独立的 CPU 探针，不接入主程序，不需要图形 SDK 或 GPU 计算。
foreach(flag JPH_USE_DX12 JPH_USE_VK JPH_USE_MTL JPH_USE_CPU_COMPUTE
    JPH_BUILD_SHARED_LIBS INTERPROCEDURAL_OPTIMIZATION USE_STATIC_MSVC_RUNTIME_LIBRARY
    OVERRIDE_CXX_FLAGS ENABLE_ALL_WARNINGS GENERATE_DEBUG_SYMBOLS ENABLE_INSTALL
    DEBUG_RENDERER_IN_DEBUG_AND_RELEASE PROFILER_IN_DEBUG_AND_RELEASE
    FLOATING_POINT_EXCEPTIONS_ENABLED)
  set(${flag} OFF CACHE BOOL "探针配置" FORCE)
endforeach()
add_subdirectory("${DFV_JOLT_SOURCE}/Build" jolt)
add_executable(DfvPhysicsProbe main.cpp)
target_link_libraries(DfvPhysicsProbe PRIVATE Jolt)
if(MSVC)
  target_compile_options(DfvPhysicsProbe PRIVATE /utf-8 /EHsc)
endif()
