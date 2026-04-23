#include "TerrainSystem.h"
#include "JobSystem.h"
#include "TerrainMeshColoring.h"
#include "TerrainGenerator.h"
#include "terrain/TerrainMeshBuilder.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <thread>
#include <utility>

TerrainSystem::TerrainSystem()
    : m_rng(1337),
      m_generationRevision(1),
      m_jobsQueuedTotal(0),
      m_jobsGeneratedTotal(0),
      m_jobsAppliedTotal(0),
      m_jobsDiscardedTotal(0) {
    m_genCtx.worldSeed = 1337;
}

TerrainSystem::~TerrainSystem() {
    WaitForActiveJobs();
}

bool TerrainSystem::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue, CopyEngine& copyCtx, JobSystem& jobSystem) {
    m_commandQueue = queue;
    m_copyCtx = &copyCtx;
    m_jobSystem = &jobSystem;

    if (!RebuildTerrainWorld(device, queue)) {
        return false;
    }

    RefreshProfiles();
    MarkAllDirty();
    return true;
}

bool TerrainSystem::ApplyResolution(ID3D12Device* device) {
    WaitForActiveJobs();
    m_worldSettings.cellsPerChunk = m_worldSettings.pendingResolution;

    if (!RebuildTerrainWorld(device, m_commandQueue)) {
        return false;
    }

    MarkAllDirty();
    return true;
}

void TerrainSystem::Update(float cameraX, float cameraZ, uint32_t frameIndex) {
    ProcessCompletedJobs(frameIndex);
    QueueDirtyJobs(cameraX, cameraZ);
}

void TerrainSystem::RecordUploads(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex) {
    m_renderer.RecordUploads(cmdList, frameIndex);
}

void TerrainSystem::Draw(ID3D12GraphicsCommandList* cmdList, const Frustum& frustum, const TerrainWorldSettings& ws) {
    m_lastChunksDrawn = 0;
    m_lastChunksCulled = 0;

    for (size_t i = 0; i < m_chunks.size(); i++) {
        const auto& chunk = *m_chunks[i];

        // Build AABB for this chunk
        AABB box;
        box.minX = chunk.worldX;
        box.minZ = chunk.worldZ;
        box.maxX = chunk.worldX + ws.chunkWorldSize;
        box.maxZ = chunk.worldZ + ws.chunkWorldSize;
        box.minY = chunk.minY;
        box.maxY = chunk.maxY;

        if (!frustum.TestAABB(box)) {
            m_lastChunksCulled++;
            continue;
        }

        m_renderer.DrawOne(cmdList, i);
        m_lastChunksDrawn++;
    }
}

void TerrainSystem::MarkAllDirty() {
    m_generationRevision.fetch_add(1, std::memory_order_acq_rel);

    for (auto& chunk : m_chunks) {
        chunk->needsUpdate = true;
        chunk->queuedRevision = 0;
    }
}

TerrainTelemetry TerrainSystem::GetTelemetry() const {
    TerrainTelemetry telemetry;
    telemetry.jobsQueued = m_jobsQueuedTotal.load(std::memory_order_acquire);
    telemetry.jobsGenerated = m_jobsGeneratedTotal.load(std::memory_order_acquire);
    telemetry.jobsApplied = m_jobsAppliedTotal.load(std::memory_order_acquire);
    telemetry.jobsDiscarded = m_jobsDiscardedTotal.load(std::memory_order_acquire);
    telemetry.pendingJobs = static_cast<uint64_t>(m_activeJobs.load(std::memory_order_acquire));
    telemetry.workerRunning = m_jobSystem != nullptr;
    telemetry.workerCount = m_jobSystem ? m_jobSystem->GetWorkerCount() : 0;
    telemetry.chunksDrawn = m_lastChunksDrawn;
    telemetry.chunksCulled = m_lastChunksCulled;
    return telemetry;
}

void TerrainSystem::RefreshProfiles() {
    m_profileFiles = ListTerrainProfiles();
}

bool TerrainSystem::SaveProfile(const std::string& filename) {
    if (!SaveTerrainProfile(filename, m_params)) {
        return false;
    }

    RefreshProfiles();
    return true;
}

bool TerrainSystem::LoadProfile(const std::string& filename) {
    if (!LoadTerrainProfile(filename, m_params)) {
        return false;
    }

    MarkAllDirty();
    return true;
}

void TerrainSystem::RandomizeSeed() {
    m_genCtx.worldSeed = m_rng();
    MarkAllDirty();
}

void TerrainSystem::ResetDefaults() {
    m_params = TerrainParams{};
    m_genCtx.worldSeed = 1337;
    MarkAllDirty();
}

// ============================================================
// Job Processing (runs on JobSystem worker threads)
// ============================================================

void TerrainSystem::ProcessTerrainJob(const TerrainJob& job, std::stop_token stoken) {
    thread_local TerrainGenerator terrainGenerator;
    thread_local uint64_t generatorRevision = 0;
    struct ActiveJobGuard {
        std::atomic<int32_t>& counter;
        ~ActiveJobGuard() {
            counter.fetch_sub(1, std::memory_order_acq_rel);
            counter.notify_all();
        }
    } guard{m_activeJobs};

    // Check stale (pre-generation)
    const uint64_t latestRevision = m_generationRevision.load(std::memory_order_acquire);
    if (job.revision != latestRevision) {
        m_jobsDiscardedTotal.fetch_add(1, std::memory_order_acq_rel);
        return;
    }

    // Re-initialize terrain generator if revision changed
    if (generatorRevision != job.revision) {
        if (!terrainGenerator.Initialize(job.params)) {
            m_jobsDiscardedTotal.fetch_add(1, std::memory_order_acq_rel);
            return;
        }
        generatorRevision = job.revision;
    }

    TerrainJobResult result;
    result.chunkIndex = job.chunkIndex;
    result.revision = job.revision;

    float minY = 0.0f;
    float maxY = 0.0f;
    const HeightfieldData heightfield = terrainGenerator.GenerateChunk(
        job.genCtx,
        job.cellsPerChunk,
        job.cellSize,
        job.worldX,
        job.worldZ,
        minY,
        maxY);

    const HeightfieldMeshBuildOptions buildOptions{
        job.worldX,
        job.worldZ,
        HeightfieldBorderMode::PaddedOneSample,
        true
    };
    result.meshData = TerrainMeshBuilder::BuildHeightfieldMeshStreams(heightfield, buildOptions);
    InteractiveTerrainMeshColoring::ApplyHeightTerrainColors(result.meshData, job.params);
    result.minY = minY;
    result.maxY = maxY;

    // Check stale (post-generation)
    if (job.revision != m_generationRevision.load(std::memory_order_acquire)) {
        m_jobsDiscardedTotal.fetch_add(1, std::memory_order_acq_rel);
        return;
    }

    if (!m_resultQueue.push(std::move(result), stoken)) {
        m_jobsDiscardedTotal.fetch_add(1, std::memory_order_acq_rel);
        return;
    }
    m_jobsGeneratedTotal.fetch_add(1, std::memory_order_acq_rel);
}

void TerrainSystem::WaitForActiveJobs() {
    // Bump revision to invalidate pending terrain jobs in the global queue
    m_generationRevision.fetch_add(1, std::memory_order_release);

    // Wait for all active terrain jobs to complete or discard
    TerrainJobResult discarded;
    while (true) {
        while (m_resultQueue.try_pop(discarded)) {
            // Results are obsolete during shutdown / rebuild.
        }

        int32_t active = m_activeJobs.load(std::memory_order_acquire);
        if (active <= 0) {
            break;
        }

        // Sleep until some worker decrements the counter.
        m_activeJobs.wait(active, std::memory_order_acquire);
    }
}

// ============================================================
// Job Scheduling + Result Processing
// ============================================================

void TerrainSystem::ProcessCompletedJobs(uint32_t frameIndex) {
    TerrainJobResult result;
    const uint64_t currentRevision = m_generationRevision.load(std::memory_order_acquire);

    while (m_resultQueue.try_pop(result)) {
        if (result.revision != currentRevision) {
            m_jobsDiscardedTotal.fetch_add(1, std::memory_order_acq_rel);
            continue;
        }

        if (result.chunkIndex >= m_chunks.size()) {
            continue;
        }

        auto& chunk = *m_chunks[result.chunkIndex];
        chunk.needsUpdate = false;
        chunk.queuedRevision = 0;
        chunk.appliedRevision = result.revision;
        chunk.minY = result.minY;
        chunk.maxY = result.maxY;

        m_renderer.StageMesh(chunk.index, result.meshData, frameIndex);
        m_jobsAppliedTotal.fetch_add(1, std::memory_order_acq_rel);
    }
}

void TerrainSystem::QueueDirtyJobs(float cameraX, float cameraZ) {
    const int updateBudget = static_cast<int>(m_jobSystem->GetWorkerCount()) * 2;
    const uint64_t currentRevision = m_generationRevision.load(std::memory_order_acquire);

    std::vector<TerrainChunkData*> dirtyChunks;
    dirtyChunks.reserve(m_chunks.size());

    for (auto& chunk : m_chunks) {
        if (chunk->needsUpdate && chunk->queuedRevision != currentRevision) {
            dirtyChunks.push_back(chunk.get());
        }
    }

    if (dirtyChunks.empty()) {
        return;
    }

    // Sort by distance to camera — closest chunks regenerated first
    std::sort(dirtyChunks.begin(), dirtyChunks.end(), [&](TerrainChunkData* a, TerrainChunkData* b) {
        const float dxA = a->worldX - cameraX;
        const float dzA = a->worldZ - cameraZ;
        const float dxB = b->worldX - cameraX;
        const float dzB = b->worldZ - cameraZ;
        const float distA = dxA * dxA + dzA * dzA;
        const float distB = dxB * dxB + dzB * dzB;
        return distA < distB;
    });

    int queued = 0;
    for (TerrainChunkData* chunk : dirtyChunks) {
        if (queued >= updateBudget) {
            break;
        }

        TerrainJob job;
        job.chunkIndex = chunk->index;
        job.revision = currentRevision;
        job.params = m_params;
        job.genCtx = m_genCtx;
        job.cellsPerChunk = m_worldSettings.cellsPerChunk;
        job.cellSize = m_worldSettings.cellSize;
        job.worldX = chunk->worldX;
        job.worldZ = chunk->worldZ;

        m_activeJobs.fetch_add(1, std::memory_order_acq_rel);
        if (!m_jobSystem->TryExecute([this, job = std::move(job)](std::stop_token stoken) {
            ProcessTerrainJob(job, stoken);
        })) {
            m_activeJobs.fetch_sub(1, std::memory_order_acq_rel);
            break;
        }
        m_jobsQueuedTotal.fetch_add(1, std::memory_order_acq_rel);
        chunk->queuedRevision = currentRevision;
        queued++;
    }
}

bool TerrainSystem::RebuildTerrainWorld(ID3D12Device* device, ID3D12CommandQueue* queue) {
    InitializeTerrainWorld(m_chunks, m_worldSettings);
    const std::vector<MeshDataStreams> initialMeshes = BuildInitialTerrainMeshes(m_chunks, m_worldSettings);

    if (!m_renderer.Initialize(device, queue, *m_copyCtx, initialMeshes)) {
        m_chunks.clear();
        return false;
    }

    return true;
}
