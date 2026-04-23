#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>

// Lightweight CPU frame profiler using QueryPerformanceCounter.
// Usage: call Begin()/End() around each phase, then read results.
struct FrameProfiler {

    void Init() {
        QueryPerformanceFrequency(&m_freq);
    }

    void FrameStart() {
        QueryPerformanceCounter(&m_frameStart);
    }

    void FrameEnd() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        frameMs = ToMs(m_frameStart, now);
    }

    void BeginTerrainUpdate() { QueryPerformanceCounter(&m_terrainUpdateStart); }
    void EndTerrainUpdate() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        terrainUpdateMs = ToMs(m_terrainUpdateStart, now);
    }

    void BeginTerrainUpload() { QueryPerformanceCounter(&m_terrainUploadStart); }
    void EndTerrainUpload() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        terrainUploadMs = ToMs(m_terrainUploadStart, now);
    }

    void BeginGBufferPass() { QueryPerformanceCounter(&m_gbufferStart); }
    void EndGBufferPass() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        gbufferPassMs = ToMs(m_gbufferStart, now);
    }

    void BeginLightingPass() { QueryPerformanceCounter(&m_lightingStart); }
    void EndLightingPass() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        lightingPassMs = ToMs(m_lightingStart, now);
    }

    void BeginImGuiPass() { QueryPerformanceCounter(&m_imguiStart); }
    void EndImGuiPass() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        imguiPassMs = ToMs(m_imguiStart, now);
    }

    // Results (updated each frame)
    float frameMs = 0.0f;
    float terrainUpdateMs = 0.0f;
    float terrainUploadMs = 0.0f;
    float gbufferPassMs = 0.0f;
    float lightingPassMs = 0.0f;
    float imguiPassMs = 0.0f;

private:
    float ToMs(const LARGE_INTEGER& start, const LARGE_INTEGER& end) const {
        return static_cast<float>(end.QuadPart - start.QuadPart) * 1000.0f
               / static_cast<float>(m_freq.QuadPart);
    }

    LARGE_INTEGER m_freq = {};
    LARGE_INTEGER m_frameStart = {};
    LARGE_INTEGER m_terrainUpdateStart = {};
    LARGE_INTEGER m_terrainUploadStart = {};
    LARGE_INTEGER m_gbufferStart = {};
    LARGE_INTEGER m_lightingStart = {};
    LARGE_INTEGER m_imguiStart = {};
};
