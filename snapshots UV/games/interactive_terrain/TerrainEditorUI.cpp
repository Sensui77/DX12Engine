#include "TerrainEditorUI.h"

#include "Camera.h"
#include "CommandContext.h"
#include "CoreWindow.h"
#include "DX12Device.h"
#include "FrameProfiler.h"
#include "GpuProfiler.h"
#include "TerrainSystem.h"
#include "imgui.h"

#include <cstdio>

TerrainEditorUI::TerrainEditorUI() {
    std::snprintf(m_saveFileName.data(), m_saveFileName.size(), "%s", "my_terrain");
}

void TerrainEditorUI::Initialize(const CoreWindow& window, Camera& camera) {
    glfwSetInputMode(window.GetGlfwWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    camera.SetActive(true);
}

void TerrainEditorUI::HandleInput(const CoreWindow& window, Camera& camera) {
    const bool escNow = glfwGetKey(window.GetGlfwWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escNow && !m_escWasPressed) {
        m_cursorFree = !m_cursorFree;
        m_showUI = m_cursorFree;
        const int cursorMode = m_cursorFree ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
        glfwSetInputMode(window.GetGlfwWindow(), GLFW_CURSOR, cursorMode);
        camera.SetActive(!m_cursorFree);
    }
    m_escWasPressed = escNow;
}

void TerrainEditorUI::UpdateCamera(const CoreWindow& window, Camera& camera, float deltaTime, bool wantsCaptureMouse) {
    if (!wantsCaptureMouse) {
        camera.Update(window.GetGlfwWindow(), deltaTime);
    }
}

TerrainEditorUIResult TerrainEditorUI::Draw(TerrainSystem& terrainSystem,
                                            CommandContext& context,
                                            DX12Device& device,
                                            const FrameProfiler& cpuProfiler,
                                            const GpuProfiler& gpuProfiler,
                                            float deltaTime) {
    TerrainEditorUIResult result;

    if (!m_showUI) {
        return result;
    }

    auto& params = terrainSystem.Params();
    auto& genCtx = terrainSystem.Context();
    auto& worldSettings = terrainSystem.WorldSettings();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(340, 600), ImGuiCond_Always);
    ImGui::Begin("Antigravity Terrain Controls", &m_showUI, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Procedural Parameters");
    if (ImGui::SliderFloat("Height Scale", &params.heightScale, 0.0f, 200.0f)) result.needsTerrainUpdate = true;
    if (ImGui::SliderFloat("Feature Scale", &params.featureScale, 10.0f, 500.0f)) result.needsTerrainUpdate = true;
    if (ImGui::SliderInt("Octaves", &params.octaves, 1, 12)) result.needsTerrainUpdate = true;
    if (ImGui::SliderFloat("Gain (Rugosity)", &params.gain, 0.0f, 1.0f)) result.needsTerrainUpdate = true;
    if (ImGui::SliderFloat("Lacunarity", &params.lacunarity, 1.0f, 5.0f)) result.needsTerrainUpdate = true;
    if (ImGui::SliderFloat("Ridged Weight", &params.ridgedWeight, 0.0f, 2.0f)) result.needsTerrainUpdate = true;
    if (ImGui::SliderFloat("Billow Weight", &params.billowWeight, 0.0f, 2.0f)) result.needsTerrainUpdate = true;
    if (ImGui::SliderFloat("Billow Scale", &params.billowFeatureScale, 10.0f, 1000.0f)) result.needsTerrainUpdate = true;

    ImGui::Separator();
    if (ImGui::Checkbox("Enable Domain Warping", &params.warpEnabled)) result.needsTerrainUpdate = true;
    if (params.warpEnabled) {
        if (ImGui::SliderFloat("Warp Amplitude", &params.warpAmplitude, 0.0f, 200.0f)) result.needsTerrainUpdate = true;
        if (ImGui::SliderFloat("Warp Scale", &params.warpFeatureScale, 10.0f, 1000.0f)) result.needsTerrainUpdate = true;

        ImGui::Indent();
        ImGui::Text("Recursive Domain Warp (Lvl 2)");
        if (ImGui::SliderFloat("Recursive Strength", &params.warpRecursiveAmplitude, 0.0f, 200.0f)) result.needsTerrainUpdate = true;
        ImGui::Unindent();
    }

    ImGui::Separator();
    ImGui::Text("Advanced Geomorphology");
    if (ImGui::SliderFloat("Ridged Multiplier", &params.ridgedMultiWeight, 0.0f, 2.0f)) result.needsTerrainUpdate = true;

    ImGui::Separator();
    ImGui::Text("Mesh & Resolution");
    ImGui::SliderInt("Resolution", &worldSettings.pendingResolution, 16, 256);
    if (ImGui::Button("Apply Resolution", ImVec2(-1, 0))) {
        context.WaitForGPU(device.GetDirectQueue());
        if (!terrainSystem.ApplyResolution(device.GetDevice())) {
            result.shouldContinue = false;
        }
    }

    ImGui::Separator();
    ImGui::Text("Seed: %llu", genCtx.worldSeed);
    if (ImGui::Button("Randomize Seed", ImVec2(-1, 0))) {
        terrainSystem.RandomizeSeed();
        result.needsTerrainUpdate = true;
    }

    if (ImGui::Button("Reset to Defaults", ImVec2(-1, 0))) {
        terrainSystem.ResetDefaults();
        result.needsTerrainUpdate = true;
    }

    ImGui::Separator();
    ImGui::Text("Profile Manager");

    ImGui::InputText("Name", m_saveFileName.data(), static_cast<size_t>(m_saveFileName.size()));
    if (ImGui::Button("Save Profile", ImVec2(-1, 0))) {
        if (terrainSystem.SaveProfile(m_saveFileName.data())) {
            m_selectedProfileIdx = -1;
        }
    }

    ImGui::Text("Existing Profiles:");
    const auto& profileFiles = terrainSystem.Profiles();
    if (ImGui::BeginListBox("##Profiles", ImVec2(-1, 120))) {
        for (int i = 0; i < static_cast<int>(profileFiles.size()); i++) {
            const bool isSelected = (m_selectedProfileIdx == i);
            if (ImGui::Selectable(profileFiles[i].c_str(), isSelected)) {
                m_selectedProfileIdx = i;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndListBox();
    }

    if (m_selectedProfileIdx != -1 && ImGui::Button("Load Selected", ImVec2(-1, 0))) {
        if (m_selectedProfileIdx < static_cast<int>(profileFiles.size()) &&
            terrainSystem.LoadProfile(profileFiles[m_selectedProfileIdx])) {
            result.needsTerrainUpdate = true;
        }
    }
    if (ImGui::Button("Refresh List", ImVec2(-1, 0))) {
        terrainSystem.RefreshProfiles();
    }

    ImGui::Separator();
    ImGui::Text("Chunks: %d (%dx%d)", static_cast<int>(terrainSystem.ChunkCount()), worldSettings.chunksX, worldSettings.chunksZ);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();

    m_terrainRateSampleTime += deltaTime;
    const TerrainTelemetry telemetry = terrainSystem.GetTelemetry();
    if (m_terrainRateSampleTime >= 0.5f) {
        m_terrainChunksPerSec = static_cast<float>(telemetry.jobsApplied - m_lastAppliedChunkTotal) / m_terrainRateSampleTime;
        m_lastAppliedChunkTotal = telemetry.jobsApplied;
        m_terrainRateSampleTime = 0.0f;
    }

    ImGui::SetNextWindowPos(ImVec2(370, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(280, 180), ImGuiCond_Always);
    ImGui::Begin("Terrain Perf", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::Text("Frame: %.2f ms", deltaTime * 1000.0f);
    ImGui::Text("Workers: %u %s", telemetry.workerCount, telemetry.workerRunning ? "(running)" : "(stopped)");
    ImGui::Text("Pending jobs: %llu", static_cast<unsigned long long>(telemetry.pendingJobs));
    ImGui::Text("Ready results: %llu", static_cast<unsigned long long>(telemetry.jobsGenerated - telemetry.jobsApplied));
    ImGui::Text("Applied total: %llu", static_cast<unsigned long long>(telemetry.jobsApplied));
    ImGui::Text("Terrain chunks/s: %.2f", m_terrainChunksPerSec);
    ImGui::Separator();
    ImGui::Text("Drawn: %u | Culled: %u", telemetry.chunksDrawn, telemetry.chunksCulled);
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(370, 200), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(280, 220), ImGuiCond_Always);
    ImGui::Begin("Profiler", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::Text("--- CPU (ms) ---");
    ImGui::Text("Frame:          %.3f", cpuProfiler.frameMs);
    ImGui::Text("TerrainUpdate:  %.3f", cpuProfiler.terrainUpdateMs);
    ImGui::Text("TerrainUpload:  %.3f", cpuProfiler.terrainUploadMs);
    ImGui::Text("G-Buffer rec:   %.3f", cpuProfiler.gbufferPassMs);
    ImGui::Text("Lighting rec:   %.3f", cpuProfiler.lightingPassMs);
    ImGui::Text("ImGui rec:      %.3f", cpuProfiler.imguiPassMs);
    ImGui::Separator();
    ImGui::Text("--- GPU (ms) ---");
    ImGui::Text("Upload/Copy:    %.3f", gpuProfiler.uploadGpuMs);
    ImGui::Text("G-Buffer:       %.3f", gpuProfiler.gbufferGpuMs);
    ImGui::Text("Lighting:       %.3f", gpuProfiler.lightingGpuMs);
    ImGui::End();

    return result;
}
