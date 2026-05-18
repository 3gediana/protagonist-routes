#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace neuro::runtime {

class EvaluationPool {
public:
    explicit EvaluationPool(std::size_t num_threads = 0);
    ~EvaluationPool();

    EvaluationPool(const EvaluationPool&) = delete;
    EvaluationPool& operator=(const EvaluationPool&) = delete;

    using Task = std::function<void()>;

    void enqueue(Task task);

    void waitAll();

    std::size_t size() const { return workers_.size(); }

private:
    void workerLoop();

    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable done_cv_;
    std::queue<Task> queue_;
    std::vector<std::thread> workers_;
    std::size_t pending_{0};
    bool stop_requested_{false};
};

}  // namespace neuro::runtime
