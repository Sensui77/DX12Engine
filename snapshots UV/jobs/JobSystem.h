#pragma once

#include <vector>
#include <memory>
#include <future>
#include <thread>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <stdexcept>
#include <atomic>
#include "ConcurrentQueue.h"

// Engine-wide job system. Owns a pool of worker threads shared by all subsystems.
// Accepts any callable via type-erased IJob interface.
class JobSystem {
public:
    // threadCount = 0 means auto-detect (hardware_concurrency - 1, min 1)
    explicit JobSystem(uint32_t threadCount = 0);
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    // Fire-and-forget job submission. Non-blocking.
    // Returns false if the global queue is full (caller should retry next frame).
    template<typename F>
    bool TryExecute(F&& func);

    // Submit a job and get a future for the result. Blocks if queue is full.
    template<typename F, typename R = std::invoke_result_t<std::decay_t<F>>>
    std::future<R> Submit(F&& func);

    uint32_t GetWorkerCount() const { return m_workerCount; }

    void Shutdown();

private:
    std::atomic<bool> m_isRunning{true};
    struct IJob {
        virtual ~IJob() = default;
        virtual void Execute(std::stop_token stoken) = 0;
    };

    template<typename F>
    struct Job : IJob {
        F func;
        explicit Job(F&& f) : func(std::move(f)) {}
        void Execute(std::stop_token stoken) override {
            if constexpr (std::is_invocable_v<F, std::stop_token>) {
                func(stoken);
            } else {
                func();
            }
        }
    };

    void WorkerMain(std::stop_token stoken);

    std::vector<std::jthread> m_workers;
    ConcurrentQueue<std::unique_ptr<IJob>> m_jobQueue{256};
    uint32_t m_workerCount = 0;
    std::atomic<bool> m_running{true};
};

// ============================================================
// Template implementations (must be in header)
// ============================================================

template<typename F>
bool JobSystem::TryExecute(F&& func) {
    if (!m_running.load(std::memory_order_acquire)) {
        return false;
    }
    using DecayF = std::decay_t<F>;
    auto job = std::make_unique<Job<DecayF>>(std::forward<F>(func));
    return m_jobQueue.try_push(std::move(job));
}

template<typename F, typename R>
std::future<R> JobSystem::Submit(F&& func) {
    if (!m_running.load(std::memory_order_acquire)) {
        std::promise<R> promise;
        auto future = promise.get_future();
        promise.set_exception(std::make_exception_ptr(std::runtime_error("JobSystem is shut down")));
        return future;
    }

    std::packaged_task<R()> task(std::forward<F>(func));
    auto future = task.get_future();

    auto wrapper = [t = std::move(task)]() mutable { t(); };
    using WrapperT = decltype(wrapper);

    auto job = std::make_unique<Job<WrapperT>>(std::move(wrapper));
    m_jobQueue.push(std::move(job));
    return future;
}
