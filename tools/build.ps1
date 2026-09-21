param([ValidateSet('Release','Debug')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$cmake='C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
Push-Location $projectRoot
try {
    & $cmake -S . -B build -G 'Visual Studio 17 2022' -A x64
    if($LASTEXITCODE -ne 0) {throw 'CMake 配置失败'}
    & $cmake --build build --config $Configuration --target CyclesViewportBench DazFastViewer CameraMailboxTest SceneIRTest MorphRuntimeTest MorphCatalogTest MorphCompatibilityTest ProjectSettingsTest SkeletonPoseTest FormulaRuntimeTest SceneWorkflowTest ConformRuntimeTest --parallel 6
    if($LASTEXITCODE -ne 0) {throw '编译失败'}
    & (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir build -C $Configuration -R '^(camera_mailbox|scene_ir_dson|morph_runtime|morph_catalog|morph_compatibility|project_settings|skeleton_pose|formula_runtime|scene_workflow|conform_runtime)$' --output-on-failure
    if($LASTEXITCODE -ne 0) {throw '相机回归测试失败'}
    if($Configuration -eq 'Release') {
        python tools/stage_runtime.py
        if($LASTEXITCODE -ne 0) {throw '运行文件暂存失败'}
    }
} finally {Pop-Location}
