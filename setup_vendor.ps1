$ErrorActionPreference = "Stop"

# Setup root folders
if (Test-Path "vendor/include") { Remove-Item -Recurse -Force "vendor/include" }
if (Test-Path "vendor/lib") { Remove-Item -Recurse -Force "vendor/lib" }
New-Item -ItemType Directory -Force -Path "vendor/include" | Out-Null
New-Item -ItemType Directory -Force -Path "vendor/lib" | Out-Null

# ==========================================
# 1. GLFW 3.4
# ==========================================
$GLFW_URL = "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip"
$ZIP_PATH = "glfw_temp.zip"
$EXTRACT_DIR = "glfw_extracted"

Write-Host "[1/3] Configurando GLFW 3.4..." -ForegroundColor Cyan
Invoke-WebRequest -Uri $GLFW_URL -OutFile $ZIP_PATH
Expand-Archive -Path $ZIP_PATH -DestinationPath $EXTRACT_DIR -Force
Copy-Item -Recurse -Path "$EXTRACT_DIR/glfw-3.4.bin.WIN64/include/GLFW" -Destination "vendor/include/GLFW"
Copy-Item -Path "$EXTRACT_DIR/glfw-3.4.bin.WIN64/lib-vc2022/glfw3.lib" -Destination "vendor/lib/"
Remove-Item -Force $ZIP_PATH
Remove-Item -Recurse -Force $EXTRACT_DIR

# ==========================================
# 2. FastNoise2 (Existing Zip)
# ==========================================
$FN_ZIP = "FastNoise2-v1.1.1-Win64-ClangCL.zip"
$FN_EXTRACT = "fastnoise_temp"

Write-Host "[2/3] Configurando FastNoise2..." -ForegroundColor Cyan
if (Test-Path $FN_ZIP) {
    Expand-Archive -Path $FN_ZIP -DestinationPath $FN_EXTRACT -Force
    Copy-Item -Recurse -Path "$FN_EXTRACT/FastNoise2/include/*" -Destination "vendor/include/"
    Copy-Item -Path "$FN_EXTRACT/FastNoise2/lib/FastNoise.lib" -Destination "vendor/lib/"
    Remove-Item -Recurse -Force $FN_EXTRACT
} else {
    Write-Warning "AVISO: $FN_ZIP não encontrado. Pulei o setup do FastNoise."
}

# ==========================================
# 3. Dear ImGui v1.91.4
# ==========================================
$IMGUI_URL = "https://github.com/ocornut/imgui/archive/refs/tags/v1.91.4.zip"
$IMGUI_ZIP = "imgui_temp.zip"
$IMGUI_EXTRACT = "imgui_extracted"

Write-Host "[3/3] Configurando Dear ImGui..." -ForegroundColor Cyan
Invoke-WebRequest -Uri $IMGUI_URL -OutFile $IMGUI_ZIP
Expand-Archive -Path $IMGUI_ZIP -DestinationPath $IMGUI_EXTRACT -Force

if (Test-Path "vendor/imgui") { Remove-Item -Recurse -Force "vendor/imgui" }
New-Item -ItemType Directory -Force -Path "vendor/imgui" | Out-Null
New-Item -ItemType Directory -Force -Path "vendor/imgui/backends" | Out-Null

$ROOT = "$IMGUI_EXTRACT/imgui-1.91.4"
# Core
Copy-Item -Path "$ROOT/*.h", "$ROOT/*.cpp" -Destination "vendor/imgui/"
# Backends
Copy-Item -Path "$ROOT/backends/imgui_impl_glfw.h", "$ROOT/backends/imgui_impl_glfw.cpp" -Destination "vendor/imgui/backends/"
Copy-Item -Path "$ROOT/backends/imgui_impl_dx12.h", "$ROOT/backends/imgui_impl_dx12.cpp" -Destination "vendor/imgui/backends/"

# ==========================================
# 4. nlohmann/json v3.11.3
# ==========================================
$JSON_URL = "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp"

Write-Host "[4/4] Configurando nlohmann/json..." -ForegroundColor Cyan
if (!(Test-Path "vendor/include/nlohmann")) { New-Item -ItemType Directory -Force -Path "vendor/include/nlohmann" | Out-Null }
Invoke-WebRequest -Uri $JSON_URL -OutFile "vendor/include/nlohmann/json.hpp"

Remove-Item -Force $IMGUI_ZIP
Remove-Item -Recurse -Force $IMGUI_EXTRACT

Write-Host "✅ SUCESSO! Todas as dependências foram configuradas." -ForegroundColor Green
