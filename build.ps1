$ErrorActionPreference = "Stop"

Write-Host "============= ANTIGRAVITY ENGINE BUILD (CLANG) =============" -ForegroundColor Yellow

# Verifica se a pasta bin existe
if (-not (Test-Path "bin")) {
    New-Item -ItemType Directory -Path "bin" | Out-Null
}

# Copia shaders para a pasta bin para que o executável ache
if (-not (Test-Path "bin/Shaders")) {
    New-Item -ItemType Directory -Path "bin/Shaders" | Out-Null
}
Copy-Item -Path "src/Shaders/*" -Destination "bin/Shaders/" -Recurse -Force


$COMPILER = "clang-cl"
# /MD garante que nosso C-Runtime usa a mesma versão DLL (MSVCRT) que a GLFW3 usou quando foi compila.
$FLAGS = "-m64 /std:c++20 /O2 /arch:AVX2 /MD /EHsc /DFASTNOISE_STATIC_LIB"
$INCLUDES = "/I src /I src/core /I src/rendering /I src/platform /I src/jobs /I src/games/interactive_terrain /I src/Shaders /I vendor/include /I vendor/imgui /I vendor/imgui/backends"

$srcCpps = Get-ChildItem -Path "src" -Recurse -Filter *.cpp | ForEach-Object { '"' + $_.FullName + '"' }
$imguiCpps = @(
    Get-ChildItem -Path "vendor/imgui" -Filter *.cpp -File | ForEach-Object { '"' + $_.FullName + '"' }
    Get-ChildItem -Path "vendor/imgui/backends" -Filter *.cpp -File | ForEach-Object { '"' + $_.FullName + '"' }
)
$SOURCES = @($srcCpps + $imguiCpps) -join " "

# DXC é carregado dinamicamente (LoadLibrary) — não precisa de d3dcompiler.lib
# ole32.lib necessário para COM (CoCreateInstance usado pelo DXC internamente)
$LINKER_FLAGS = "/link /LIBPATH:vendor/lib FastNoise.lib glfw3.lib d3d12.lib dxgi.lib user32.lib gdi32.lib shell32.lib msvcrt.lib psapi.lib ole32.lib /SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup /OUT:bin/AntigravityEngine.exe"

$BUILD_CMD = "$COMPILER $FLAGS $INCLUDES $SOURCES $LINKER_FLAGS"

Write-Host "Iniciando compilação: $COMPILER..." -ForegroundColor Cyan
Write-Host "> $BUILD_CMD" -ForegroundColor DarkGray

# Executa o comando
Invoke-Expression $BUILD_CMD

if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ Build concluído com SUCESSO! Binário gerado em bin/AntigravityEngine.exe" -ForegroundColor Green
} else {
    Write-Host "❌ Falha no Build." -ForegroundColor Red
    exit 1
}

# ==========================================
# Copia DXC DLLs do Windows SDK para bin/
# ==========================================
Write-Host "Verificando DXC DLLs..." -ForegroundColor Cyan
$dxcInBin = Test-Path "bin/dxcompiler.dll"
if (-not $dxcInBin) {
    $sdkBinDirs = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64" -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
    $found = $false
    foreach ($dir in $sdkBinDirs) {
        $dxcPath = Join-Path $dir.FullName "dxcompiler.dll"
        $dxilPath = Join-Path $dir.FullName "dxil.dll"
        if ((Test-Path $dxcPath) -and (Test-Path $dxilPath)) {
            Copy-Item $dxcPath "bin/" -Force
            Copy-Item $dxilPath "bin/" -Force
            Write-Host "  DXC DLLs copiados de: $($dir.FullName)" -ForegroundColor Green
            $found = $true
            break
        }
    }
    if (-not $found) {
        Write-Host "⚠️  dxcompiler.dll não encontrado no Windows SDK. Copie manualmente para bin/" -ForegroundColor Yellow
    }
} else {
    Write-Host "  DXC DLLs já presentes em bin/" -ForegroundColor DarkGray
}
