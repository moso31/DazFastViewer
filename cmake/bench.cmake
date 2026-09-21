# 在 Cycles app 目录作用域定义，继承该版本所需的编译选项和 ABI。
add_executable(CyclesViewportBench "${DFV_ROOT}/src/bench/main.cpp"
  "${DFV_ROOT}/src/bench/fixtures.cpp"
  "${DFV_ROOT}/src/cycles/adapter.cpp"
  "${DFV_ROOT}/src/viewport/window.cpp"
  "${DFV_ROOT}/src/viewport/display.cpp" "${DFV_ROOT}/src/viewport/overlay.cpp")
target_include_directories(CyclesViewportBench PRIVATE "${DFV_ROOT}/src")
target_link_libraries(CyclesViewportBench PRIVATE ${LIB} dfv_scene bf::dependencies::epoxy
  opengl32 gdi32 user32 dwmapi psapi winmm)
target_compile_definitions(CyclesViewportBench PRIVATE
  NOMINMAX WIN32_LEAN_AND_MEAN DFV_CYCLES_SOURCE="${DFV_ROOT}/.deps/cycles/src")
target_compile_options(CyclesViewportBench PRIVATE /utf-8)
install(TARGETS CyclesViewportBench RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}")
install(FILES "${DFV_LIB_DIR}/epoxy/bin/epoxy-0.dll" DESTINATION "${CMAKE_INSTALL_PREFIX}")

set(DFV_QT_ROOT "C:/Qt/6.10.3/msvc2022_64" CACHE PATH "Qt MSVC x64 开发套件")
find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets PATHS "${DFV_QT_ROOT}/lib/cmake/Qt6" NO_DEFAULT_PATH)
qt_add_executable(DazFastViewer WIN32 "${DFV_ROOT}/src/editor/main.cpp"
  "${DFV_ROOT}/src/editor/project.cpp" "${DFV_ROOT}/src/editor/parameters.cpp"
  "${DFV_ROOT}/src/editor/renderer.cpp" "${DFV_ROOT}/src/bench/fixtures.cpp"
  "${DFV_ROOT}/src/cycles/adapter.cpp" "${DFV_ROOT}/src/viewport/window.cpp"
  "${DFV_ROOT}/src/viewport/display.cpp" "${DFV_ROOT}/src/viewport/overlay.cpp")
target_include_directories(DazFastViewer PRIVATE "${DFV_ROOT}/src")
target_link_libraries(DazFastViewer PRIVATE ${LIB} dfv_scene Qt6::Widgets bf::dependencies::epoxy opengl32 gdi32 user32 dwmapi psapi winmm)
target_compile_definitions(DazFastViewer PRIVATE QT_NO_KEYWORDS NOMINMAX WIN32_LEAN_AND_MEAN DFV_CYCLES_SOURCE="${DFV_ROOT}/.deps/cycles/src")
target_compile_options(DazFastViewer PRIVATE /utf-8)
add_executable(ProjectSettingsTest "${DFV_ROOT}/tests/project_settings.cpp" "${DFV_ROOT}/src/editor/project.cpp")
target_include_directories(ProjectSettingsTest PRIVATE "${DFV_ROOT}/src")
target_link_libraries(ProjectSettingsTest PRIVATE Qt6::Widgets)
target_compile_options(ProjectSettingsTest PRIVATE /utf-8)
add_test(NAME project_settings COMMAND ProjectSettingsTest)
set_tests_properties(project_settings PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${DFV_QT_ROOT}/bin")
