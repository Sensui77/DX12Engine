#include "JobSystem.h"
#include <exception>
#include <iostream>

JobSystem::JobSystem(uint32_t threadCount) {
    uint32_t cores = std::thread::hardware_concurrency();
    if (threadCount == 0) {
        threadCount = (cores > 1) ? (cores - 1) : 1;
    }
    m_workerCount = threadCount;
    m_workers.reserve(m_workerCount);

    for (uint32_t i = 0; i < m_workerCount; i++) {
        m_workers.emplace_back([this](std::stop_token stoken) {
            WorkerMain(stoken);
        });
    }

    std::cout << "[JobSystem] Started " << m_workerCount << " worker threads\n";
}

JobSystem::~JobSystem() {
    Shutdown();
}

void JobSystem::Shutdown() {
    m_running.store(false, std::memory_order_release);
    if (m_workers.empty()) return;
    // jthread destructors request stop + join automatically
    m_workers.clear();
    m_workerCount = 0;
}

void JobSystem::WorkerMain(std::stop_token stoken) {
    std::unique_ptr<IJob> job;
    while (m_jobQueue.wait_pop(job, stoken)) {
        try {
            job->Execute(stoken);
        } catch (const std::exception& e) {
            std::cerr << "[JobSystem] Job threw exception: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[JobSystem] Job threw unknown exception\n";
        }
        job.reset();
    }
}
