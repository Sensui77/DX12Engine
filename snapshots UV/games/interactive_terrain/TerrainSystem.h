#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "Frustum.h"
#include "ConcurrentQueue.h"
#include "MeshData.h"
#include "TerrainProfileIO.h"
#include "TerrainWorld.h"
#include "rendering/MeshRenderProxy.h"

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12GraphicsCommandList;
class CopyEngine;
class JobSystem;

struct TerrainJob {
    size_t chunkIndex = 0;
    uint64_t revision = 0;
    TerrainParams params;
    GenerationContext genCtx;
    int cellsPerChunk = 0;
    float cellSize = 1.0f;
    float worldX = 0.0f;
    float worldZ = 0.0f;
};

struct TerrainJobResult {
    size_t chunkIndex = 0;
    uint64_t revision = 0;
    MeshDataStreams meshData;
    float minY = 0.0f;
    float maxY = 0.0f;
};

struct TerrainTelemetry {
    uint64_t jobsQueued = 0;
    uint64_t jobsGenerated = 0;
    uint64_t jobsApplied = 0;
    uint64_t jobsDiscarded = 0;
    uint64_t pendingJobs = 0;
    bool workerRunning = false;
    uint32_t workerCount = 0;
    uint32_t chunksDrawn = 0;
    uint32_t chunksCulled = 0;
};

class TerrainSystem {
public:
    TerrainSystem();
    ~TerrainSystem();

    bool Initialize(ID3D12Device* device, ID3D12CommandQueue* queue, CopyEngine& copyCtx, JobSystem& jobSystem);
    bool ApplyResolution(ID3D12Device* device);

    void Update(float cameraX, float cameraZ, uint32_t frameIndex);
    void RecordUploads(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex);
    void Draw(ID3D12GraphicsCommandList* cmdList, const Frustum& frustum, const TerrainWorldSettings& ws);

    void MarkAllDirty();
    void RefreshProfiles();

    bool SaveProfile(const std::string& filename);
    bool LoadProfile(const std::string& filename);

    void RandomizeSeed();
    void ResetDefaults();

    TerrainParams& Params() { return m_params; }
    const TerrainParams& Params() const { return m_params; }

    GenerationContext& Context() { return m_genCtx; }
    const GenerationContext& Context() const { return m_genCtx; }

    TerrainWorldSettings& WorldSettings() { return m_worldSettings; }
    const TerrainWorldSettings& WorldSettings() const { return m_worldSettings; }

    const std::vector<std::string>& Profiles() const { return m_profileFiles; }
    size_t ChunkCount() const { return m_chunks.size(); }
    TerrainTelemetry GetTelemetry() const;

private:
    void ProcessTerrainJob(const TerrainJob& job, std::stop_token stoken);
    void WaitForActiveJobs();

    void ProcessCompletedJobs(uint32_t frameIndex);
    void QueueDirtyJobs(float cameraX, float cameraZ);
    bool RebuildTerrainWorld(ID3D12Device* device, ID3D12CommandQueue* queue);

    TerrainParams m_params;
    GenerationContext m_genCtx;
    TerrainWorldSettings m_worldSettings;

    MeshRenderProxy m_renderer;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    CopyEngine* m_copyCtx = nullptr;

    std::vector<std::unique_ptr<TerrainChunkData>> m_chunks;
    std::vector<std::string> m_profileFiles;
    std::mt19937 m_rng;

    std::atomic<uint64_t> m_generationRevision;
    std::atomic<uint64_t> m_jobsQueuedTotal;
    std::atomic<uint64_t> m_jobsGeneratedTotal;
    std::atomic<uint64_t> m_jobsAppliedTotal;
    std::atomic<uint64_t> m_jobsDiscardedTotal;

    JobSystem* m_jobSystem = nullptr;
    std::atomic<int32_t> m_activeJobs{0};

    ConcurrentQueue<TerrainJobResult> m_resultQueue{64};

    // Frustum culling stats (per frame)
    uint32_t m_lastChunksDrawn = 0;
    uint32_t m_lastChunksCulled = 0;
};
