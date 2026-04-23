#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <stop_token>
#include <cstddef>

// Thread-safe multi-producer multi-consumer queue with optional capacity limit.
// Uses mutex + condition_variable_any for std::stop_token support (C++20).
// maxCapacity = 0 means unbounded (backward compatible).
template<typename T>
class ConcurrentQueue {
public:
    explicit ConcurrentQueue(size_t maxCapacity = 0) : m_maxCapacity(maxCapacity) {}

    // Blocking push. If queue is at capacity, waits until space is available.
    // Returns false if stop was requested via stoken while waiting.
    template<typename U>
    bool push(U&& value, std::stop_token stoken = {}) {
        {
            std::unique_lock lock(m_mutex);
            if (m_maxCapacity > 0) {
                auto pred = [this] { return m_queue.size() < m_maxCapacity; };
                if (stoken.stop_possible()) {
                    if (!m_cvFull.wait(lock, stoken, pred)) return false;
                } else {
                    m_cvFull.wait(lock, pred);
                }
            }
            m_queue.push_back(std::forward<U>(value));
        }
        m_cv.notify_one();
        return true;
    }

    // Non-blocking push. Returns false immediately if queue is at capacity.
    template<typename U>
    bool try_push(U&& value) {
        {
            std::lock_guard lock(m_mutex);
            if (m_maxCapacity > 0 && m_queue.size() >= m_maxCapacity) return false;
            m_queue.push_back(std::forward<U>(value));
        }
        m_cv.notify_one();
        return true;
    }

    bool try_pop(T& out) {
        {
            std::lock_guard lock(m_mutex);
            if (m_queue.empty()) return false;
            out = std::move(m_queue.front());
            m_queue.pop_front();
        }
        m_cvFull.notify_one();
        return true;
    }

    // Blocking pop with stop_token support.
    // Returns false if stop was requested (no item popped).
    bool wait_pop(T& out, std::stop_token stoken) {
        {
            std::unique_lock lock(m_mutex);
            if (!m_cv.wait(lock, stoken, [this] { return !m_queue.empty(); })) {
                return false;
            }
            out = std::move(m_queue.front());
            m_queue.pop_front();
        }
        m_cvFull.notify_one();
        return true;
    }

    bool empty() const {
        std::lock_guard lock(m_mutex);
        return m_queue.empty();
    }

    size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_queue.size();
    }

private:
    std::deque<T> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable_any m_cv;      // signals: empty → non-empty
    std::condition_variable_any m_cvFull;   // signals: full → space available
    size_t m_maxCapacity;
};
