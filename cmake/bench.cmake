# 在 Cycles app 目录作用域定义，继承该版本所需的编译选项和 ABI。
add_executable(CyclesViewportBench "${DFV_ROOT}/src/bench/main.cpp"
  "${DFV_ROOT}/src/bench/fixtures.cpp"
  "${DFV_ROOT}/src/cycles/adapter.cpp"
  "${DFV_ROOT}/src/viewport/window.cpp"
  "${DFV_ROOT}/src/viewport/display.cpp")
target_include_directories(CyclesViewportBench PRIVATE "${DFV_ROOT}/src")
target_link_libraries(CyclesViewportBench PRIVATE ${LIB} dfv_scene bf::dependencies::epoxy
  opengl32 gdi32 user32 dwmapi psapi winmm)
target_compile_definitions(CyclesViewportBench PRIVATE
  NOMINMAX WIN32_LEAN_AND_MEAN DFV_CYCLES_SOURCE="${DFV_ROOT}/.deps/cycles/src")
target_compile_options(CyclesViewportBench PRIVATE /utf-8)
install(TARGETS CyclesViewportBench RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}")
install(FILES "${DFV_LIB_DIR}/epoxy/bin/epoxy-0.dll" DESTINATION "${CMAKE_INSTALL_PREFIX}")
