#include "runtime/headless/EvaluationPool.h"

#include <cassert>
#include <stdexcept>

namespace neuro::runtime {

EvaluationPool::EvaluationPool(std::size_t num_threads) {
    if (num_threads == 0) {
        num_threads = std::max(1u, std::thread::hardware_concurrency());
        if (num_threads > 1) {
            --num_threads;
        }
    }

    stop_requested_ = false;
    workers_.reserve(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
}

EvaluationPool::~EvaluationPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
    }
    cv_.notify_all();
    done_cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) {
            w.join();
        }
    }
}

void EvaluationPool::enqueue(Task task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(task));
        ++pending_;
    }
    cv_.notify_one();
}

void EvaluationPool::waitAll() {
    std::unique_lock<std::mutex> lock(mutex_);
    done_cv_.wait(lock, [this] { return pending_ == 0 && queue_.empty(); });
}

void EvaluationPool::workerLoop() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_requested_ || !queue_.empty(); });

            if (stop_requested_ && queue_.empty()) {
                return;
            }

            if (queue_.empty()) {
                continue;
            }

            task = std::move(queue_.front());
            queue_.pop();
        }

        task();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            --pending_;
        }
        done_cv_.notify_one();
    }
}

}  // namespace neuro::runtime
