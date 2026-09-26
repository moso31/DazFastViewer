# 在 Cycles app 目录作用域定义，继承该版本所需的编译选项和 ABI。
add_executable(RenderSchedulerTest "${DFV_ROOT}/tests/render_scheduler.cpp")
target_link_libraries(RenderSchedulerTest PRIVATE ${LIB})
target_compile_options(RenderSchedulerTest PRIVATE /utf-8)
add_test(NAME render_scheduler COMMAND RenderSchedulerTest)
set_tests_properties(render_scheduler PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${DFV_ROOT}/out")
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
find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets Test PATHS "${DFV_QT_ROOT}/lib/cmake/Qt6" NO_DEFAULT_PATH)
add_library(dfv_content STATIC "${DFV_ROOT}/src/editor/content_browser.cpp" "${DFV_ROOT}/src/editor/content_catalog.cpp" "${DFV_ROOT}/src/editor/ui_scale.cpp")
target_sources(dfv_content PRIVATE "${DFV_ROOT}/src/editor/viewport_settings.cpp" "${DFV_ROOT}/src/editor/application_settings.cpp" "${DFV_ROOT}/src/editor/content_locator.cpp")
target_include_directories(dfv_content PUBLIC "${DFV_ROOT}/src" "${DFV_ROOT}/third_party")
target_link_libraries(dfv_content PUBLIC Qt6::Widgets dfv_scene)
target_compile_options(dfv_content PRIVATE /utf-8)
target_compile_definitions(dfv_content PRIVATE QT_NO_KEYWORDS NOMINMAX)
qt_add_executable(DazFastViewer WIN32 "${DFV_ROOT}/src/editor/main.cpp"
  "${DFV_ROOT}/src/editor/chrome.cpp"
  "${DFV_ROOT}/src/editor/extension_panel.cpp"
  "${DFV_ROOT}/src/editor/material_panel.cpp"
  "${DFV_ROOT}/src/editor/powerpose_panel.cpp"
  "${DFV_ROOT}/src/editor/project.cpp" "${DFV_ROOT}/src/editor/parameters.cpp"
  "${DFV_ROOT}/src/editor/renderer.cpp" "${DFV_ROOT}/src/bench/fixtures.cpp"
  "${DFV_ROOT}/src/cycles/adapter.cpp" "${DFV_ROOT}/src/viewport/window.cpp"
  "${DFV_ROOT}/src/viewport/display.cpp" "${DFV_ROOT}/src/viewport/overlay.cpp")
target_include_directories(DazFastViewer PRIVATE "${DFV_ROOT}/src")
target_link_libraries(DazFastViewer PRIVATE ${LIB} dfv_scene dfv_content Qt6::Widgets bf::dependencies::epoxy opengl32 gdi32 user32 dwmapi psapi winmm)
target_compile_definitions(DazFastViewer PRIVATE QT_NO_KEYWORDS NOMINMAX WIN32_LEAN_AND_MEAN DFV_CYCLES_SOURCE="${DFV_ROOT}/.deps/cycles/src")
target_compile_options(DazFastViewer PRIVATE /utf-8)
add_executable(RendererQualityTest EXCLUDE_FROM_ALL "${DFV_ROOT}/tests/renderer_quality.cpp"
  "${DFV_ROOT}/src/editor/renderer.cpp" "${DFV_ROOT}/src/cycles/adapter.cpp" "${DFV_ROOT}/src/bench/fixtures.cpp"
  "${DFV_ROOT}/src/viewport/window.cpp" "${DFV_ROOT}/src/viewport/display.cpp" "${DFV_ROOT}/src/viewport/overlay.cpp")
target_include_directories(RendererQualityTest PRIVATE "${DFV_ROOT}/src")
target_link_libraries(RendererQualityTest PRIVATE ${LIB} dfv_scene Qt6::Widgets bf::dependencies::epoxy opengl32 gdi32 user32 dwmapi psapi winmm)
target_compile_definitions(RendererQualityTest PRIVATE QT_NO_KEYWORDS NOMINMAX WIN32_LEAN_AND_MEAN DFV_CYCLES_SOURCE="${DFV_ROOT}/.deps/cycles/src")
target_compile_options(RendererQualityTest PRIVATE /utf-8)
add_executable(RendererMaterialsTest EXCLUDE_FROM_ALL "${DFV_ROOT}/tests/renderer_materials.cpp"
  "${DFV_ROOT}/src/editor/renderer.cpp" "${DFV_ROOT}/src/cycles/adapter.cpp" "${DFV_ROOT}/src/bench/fixtures.cpp"
  "${DFV_ROOT}/src/viewport/window.cpp" "${DFV_ROOT}/src/viewport/display.cpp" "${DFV_ROOT}/src/viewport/overlay.cpp")
target_include_directories(RendererMaterialsTest PRIVATE "${DFV_ROOT}/src")
target_link_libraries(RendererMaterialsTest PRIVATE ${LIB} dfv_scene Qt6::Widgets bf::dependencies::epoxy opengl32 gdi32 user32 dwmapi psapi winmm)
target_compile_definitions(RendererMaterialsTest PRIVATE QT_NO_KEYWORDS NOMINMAX WIN32_LEAN_AND_MEAN DFV_CYCLES_SOURCE="${DFV_ROOT}/.deps/cycles/src")
target_compile_options(RendererMaterialsTest PRIVATE /utf-8)
add_executable(ProjectSettingsTest "${DFV_ROOT}/tests/project_settings.cpp" "${DFV_ROOT}/src/editor/project.cpp" "${DFV_ROOT}/src/editor/application_settings.cpp")
target_include_directories(ProjectSettingsTest PRIVATE "${DFV_ROOT}/src" "${DFV_ROOT}/third_party")
target_link_libraries(ProjectSettingsTest PRIVATE Qt6::Widgets)
target_compile_options(ProjectSettingsTest PRIVATE /utf-8)
add_test(NAME project_settings COMMAND ProjectSettingsTest)
set_tests_properties(project_settings PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${DFV_QT_ROOT}/bin")
add_executable(ParameterControlsTest "${DFV_ROOT}/tests/parameter_controls.cpp" "${DFV_ROOT}/src/editor/parameters.cpp")
target_sources(ParameterControlsTest PRIVATE "${DFV_ROOT}/src/editor/extension_panel.cpp")
target_include_directories(ParameterControlsTest PRIVATE "${DFV_ROOT}/src")
target_link_libraries(ParameterControlsTest PRIVATE dfv_scene Qt6::Widgets Qt6::Test)
target_compile_options(ParameterControlsTest PRIVATE /utf-8)
add_test(NAME parameter_controls COMMAND ParameterControlsTest)
add_executable(MaterialsTest "${DFV_ROOT}/tests/materials.cpp" "${DFV_ROOT}/src/editor/material_panel.cpp")
target_link_libraries(MaterialsTest PRIVATE dfv_scene dfv_content Qt6::Widgets Qt6::Test)
target_compile_options(MaterialsTest PRIVATE /utf-8)
target_compile_definitions(MaterialsTest PRIVATE NOMINMAX QT_NO_KEYWORDS)
add_test(NAME materials COMMAND MaterialsTest)
set_tests_properties(materials PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${DFV_QT_ROOT}/bin")
add_executable(PowerPosePanelTest "${DFV_ROOT}/tests/powerpose_panel.cpp" "${DFV_ROOT}/src/editor/powerpose_panel.cpp")
target_link_libraries(PowerPosePanelTest PRIVATE dfv_scene Qt6::Widgets Qt6::Test)
target_compile_options(PowerPosePanelTest PRIVATE /utf-8)
target_compile_definitions(PowerPosePanelTest PRIVATE NOMINMAX QT_NO_KEYWORDS)
add_test(NAME powerpose_panel COMMAND PowerPosePanelTest)
add_executable(EditorChromeTest "${DFV_ROOT}/tests/editor_chrome.cpp" "${DFV_ROOT}/src/editor/chrome.cpp")
target_link_libraries(EditorChromeTest PRIVATE dfv_scene Qt6::Widgets Qt6::Test dwmapi)
target_compile_options(EditorChromeTest PRIVATE /utf-8)
target_compile_definitions(EditorChromeTest PRIVATE NOMINMAX QT_NO_KEYWORDS)
add_test(NAME editor_chrome COMMAND EditorChromeTest)
set_tests_properties(editor_chrome PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${DFV_QT_ROOT}/bin")
set_tests_properties(powerpose_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${DFV_QT_ROOT}/bin")
set_tests_properties(parameter_controls PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${DFV_QT_ROOT}/bin")
add_executable(ContentBrowserTest "${DFV_ROOT}/tests/content_browser.cpp")
target_link_libraries(ContentBrowserTest PRIVATE dfv_content Qt6::Test)
target_compile_options(ContentBrowserTest PRIVATE /utf-8)
add_test(NAME content_browser COMMAND ContentBrowserTest)
add_executable(UiScaleTest "${DFV_ROOT}/tests/ui_scale.cpp" "${DFV_ROOT}/src/editor/chrome.cpp")
target_link_libraries(UiScaleTest PRIVATE dfv_content dfv_scene Qt6::Test dwmapi)
target_compile_options(UiScaleTest PRIVATE /utf-8)
target_compile_definitions(UiScaleTest PRIVATE NOMINMAX QT_NO_KEYWORDS)
add_test(NAME ui_scale COMMAND UiScaleTest)
set_tests_properties(ui_scale PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${DFV_QT_ROOT}/bin")
set_tests_properties(content_browser PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${DFV_QT_ROOT}/bin")
add_executable(ViewportOverlayTest EXCLUDE_FROM_ALL "${DFV_ROOT}/tests/viewport_overlay.cpp" "${DFV_ROOT}/src/viewport/window.cpp" "${DFV_ROOT}/src/viewport/overlay.cpp")
target_sources(ViewportOverlayTest PRIVATE "${DFV_ROOT}/src/viewport/display.cpp")
target_include_directories(ViewportOverlayTest PRIVATE "${DFV_ROOT}/src")
target_link_libraries(ViewportOverlayTest PRIVATE ${LIB} dfv_scene Qt6::Gui bf::dependencies::epoxy opengl32 gdi32 user32 dwmapi winmm)
target_compile_options(ViewportOverlayTest PRIVATE /utf-8)
target_compile_definitions(ViewportOverlayTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
