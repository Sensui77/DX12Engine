#include "ShaderCompiler.h"
#include <iostream>
#include <filesystem>
#include <vector>

using Microsoft::WRL::ComPtr;

// Static members
ComPtr<IDxcUtils> ShaderCompiler::s_utils;
ComPtr<IDxcCompiler3> ShaderCompiler::s_compiler;
ComPtr<IDxcIncludeHandler> ShaderCompiler::s_includeHandler;
HMODULE ShaderCompiler::s_dxcModule = nullptr;

// DXC CLSIDs (defined here to avoid SDK version dependency issues)
namespace {
    constexpr GUID kCLSID_DxcCompiler = {0x73e22d93, 0xe6ce, 0x47f3, {0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0}};
    constexpr GUID kCLSID_DxcUtils   = {0x6245d6af, 0x66e0, 0x48fd, {0x80, 0xb4, 0x4d, 0x27, 0x17, 0x96, 0x74, 0x8c}};
}

using DxcCreateInstanceProc = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);

static std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path().wstring();
}

bool ShaderCompiler::Initialize() {
    // Try loading from exe directory first, then system PATH
    std::wstring localDxc = GetExeDirectory() + L"\\dxcompiler.dll";
    s_dxcModule = LoadLibraryW(localDxc.c_str());
    if (!s_dxcModule) {
        s_dxcModule = LoadLibraryW(L"dxcompiler.dll");
    }

    if (!s_dxcModule) {
        std::cerr << "[DXC] Failed to load dxcompiler.dll — ensure it is in the exe directory or system PATH\n";
        return false;
    }

    auto createInstance = reinterpret_cast<DxcCreateInstanceProc>(
        GetProcAddress(s_dxcModule, "DxcCreateInstance"));
    if (!createInstance) {
        std::cerr << "[DXC] DxcCreateInstance entry point not found\n";
        FreeLibrary(s_dxcModule);
        s_dxcModule = nullptr;
        return false;
    }

    if (FAILED(createInstance(kCLSID_DxcUtils, __uuidof(IDxcUtils),
                              reinterpret_cast<void**>(s_utils.GetAddressOf())))) {
        std::cerr << "[DXC] Failed to create IDxcUtils\n";
        return false;
    }

    if (FAILED(createInstance(kCLSID_DxcCompiler, __uuidof(IDxcCompiler3),
                              reinterpret_cast<void**>(s_compiler.GetAddressOf())))) {
        std::cerr << "[DXC] Failed to create IDxcCompiler3\n";
        return false;
    }

    if (FAILED(s_utils->CreateDefaultIncludeHandler(&s_includeHandler))) {
        std::cerr << "[DXC] Failed to create default include handler\n";
        return false;
    }

    std::cout << "[DXC] Shader compiler initialized (SM 6.0+)\n";
    return true;
}

void ShaderCompiler::Shutdown() {
    s_includeHandler.Reset();
    s_compiler.Reset();
    s_utils.Reset();
    if (s_dxcModule) {
        FreeLibrary(s_dxcModule);
        s_dxcModule = nullptr;
    }
}

ComPtr<IDxcBlob> ShaderCompiler::CompileFromFile(
    const std::wstring& filename,
    const std::wstring& entryPoint,
    const std::wstring& targetProfile)
{
    std::wstring fullPath = GetExeDirectory() + L"\\" + filename;

    // Load source file
    ComPtr<IDxcBlobEncoding> sourceBlob;
    HRESULT hr = s_utils->LoadFile(fullPath.c_str(), nullptr, &sourceBlob);
    if (FAILED(hr)) {
        std::cerr << "[DXC] Failed to load shader: ";
        std::wcerr << fullPath;
        std::cerr << " (HR=0x" << std::hex << hr << std::dec << ")\n";
        return nullptr;
    }

    DxcBuffer sourceBuffer = {};
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();
    sourceBuffer.Encoding = DXC_CP_UTF8;

    // Build compile arguments
    std::vector<LPCWSTR> arguments;
    arguments.push_back(L"-E");
    arguments.push_back(entryPoint.c_str());
    arguments.push_back(L"-T");
    arguments.push_back(targetProfile.c_str());
    arguments.push_back(L"-HV");
    arguments.push_back(L"2021");

#if defined(_DEBUG)
    arguments.push_back(L"-Zi");
    arguments.push_back(L"-Od");
#else
    arguments.push_back(L"-O3");
#endif

    // Compile
    ComPtr<IDxcResult> result;
    hr = s_compiler->Compile(
        &sourceBuffer,
        arguments.data(),
        static_cast<UINT32>(arguments.size()),
        s_includeHandler.Get(),
        IID_PPV_ARGS(&result));

    // Check for errors
    ComPtr<IDxcBlobUtf8> errors;
    if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr))) {
        if (errors && errors->GetStringLength() > 0) {
            std::cerr << "[DXC] Shader compilation output:\n" << errors->GetStringPointer() << "\n";
        }
    }

    HRESULT compileStatus;
    result->GetStatus(&compileStatus);
    if (FAILED(compileStatus)) {
        std::cerr << "[DXC] Compilation failed for: ";
        std::wcerr << filename;
        std::cerr << "\n";
        return nullptr;
    }

    // Get compiled DXIL bytecode
    ComPtr<IDxcBlob> shaderBlob;
    if (FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr))) {
        std::cerr << "[DXC] Failed to extract compiled bytecode\n";
        return nullptr;
    }

    return shaderBlob;
}
