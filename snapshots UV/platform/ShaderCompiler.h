#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Unknwn.h>
#include <objidl.h>
#include <dxcapi.h>
#include <string>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class ShaderCompiler {
public:
    // Load dxcompiler.dll and create compiler instances.
    // Must be called before any CompileFromFile calls.
    static bool Initialize();
    static void Shutdown();

    // Compile HLSL from file using DXC (Shader Model 6.0+).
    // Returns compiled DXIL bytecode blob, or nullptr on failure.
    static ComPtr<IDxcBlob> CompileFromFile(
        const std::wstring& filename,
        const std::wstring& entryPoint,
        const std::wstring& targetProfile);

private:
    static ComPtr<IDxcUtils> s_utils;
    static ComPtr<IDxcCompiler3> s_compiler;
    static ComPtr<IDxcIncludeHandler> s_includeHandler;
    static HMODULE s_dxcModule;
};
