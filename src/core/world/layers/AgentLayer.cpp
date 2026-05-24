#include "core/world/layers/AgentLayer.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace neuro {

class AgentLayer::DecisionExecutor {
public:
    explicit DecisionExecutor(std::size_t thread_count)
        : thread_count_(thread_count > 0 ? thread_count : 1) {
        if (thread_count_ <= 1) {
            return;
        }

        const std::size_t worker_count = thread_count_ - 1;
        workers_.reserve(worker_count);
        for (std::size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    ~DecisionExecutor() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_requested_ = true;
            ++epoch_;
        }
        cv_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void parallelFor(std::size_t count, const std::function<void(std::size_t)>& fn) {
        if (workers_.empty() || count <= 1) {
            for (std::size_t i = 0; i < count; ++i) {
                fn(i);
            }
            return;
        }

        std::size_t active_epoch = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            count_ = count;
            next_index_.store(0, std::memory_order_relaxed);
            fn_ = &fn;
            completed_workers_ = 0;
            active_epoch = ++epoch_;
        }

        cv_.notify_all();
        executeRange(fn);

        std::unique_lock<std::mutex> lock(mutex_);
        done_cv_.wait(lock, [&] { return completed_workers_ == workers_.size() && epoch_ == active_epoch; });
        fn_ = nullptr;
        count_ = 0;
    }

private:
    void executeRange(const std::function<void(std::size_t)>& fn) {
        while (true) {
            const std::size_t index = next_index_.fetch_add(1, std::memory_order_relaxed);
            if (index >= count_) {
                break;
            }
            // Workers cannot let exceptions escape without triggering
            // std::terminate (and a Windows /GS stack-overrun crash
            // surfaces as 0xC0000409). Swallow + log so the rest of the
            // generation can finish and diagnostics survive.
            try {
                fn(index);
            } catch (const std::exception& e) {
                spdlog::error("AgentLayer worker fn({}) exception: {}", index, e.what());
            } catch (...) {
                spdlog::error("AgentLayer worker fn({}) unknown exception", index);
            }
        }
    }

    void workerLoop() {
        std::size_t observed_epoch = 0;
        while (true) {
            const std::function<void(std::size_t)>* active_fn = nullptr;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [&] { return stop_requested_ || epoch_ != observed_epoch; });
                if (stop_requested_) {
                    return;
                }
                observed_epoch = epoch_;
                active_fn = fn_;
            }

            if (active_fn != nullptr) {
                executeRange(*active_fn);
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                ++completed_workers_;
            }
            done_cv_.notify_one();
        }
    }

    std::size_t thread_count_{1};
    std::vector<std::thread> workers_{};
    std::mutex mutex_{};
    std::condition_variable cv_{};
    std::condition_variable done_cv_{};
    std::atomic<std::size_t> next_index_{0};
    const std::function<void(std::size_t)>* fn_{nullptr};
    std::size_t count_{0};
    std::size_t completed_workers_{0};
    std::size_t epoch_{0};
    bool stop_requested_{false};
};

namespace {

std::size_t resolveDecisionThreads(std::size_t requested_threads) {
    if (requested_threads == 0) {
        const std::size_t hw_threads = std::max<std::size_t>(1u, std::thread::hardware_concurrency());
        return hw_threads > 1 ? hw_threads - 1 : 1;
    }
    return requested_threads;
}

}  // namespace

AgentLayer::AgentLayer(float death_threshold, float base_metabolism, std::size_t decision_threads)
    : death_threshold_(death_threshold),
      base_metabolism_(base_metabolism),
      decision_threads_(resolveDecisionThreads(decision_threads)),
      decision_executor_(decision_threads_ > 1 ? std::make_unique<DecisionExecutor>(decision_threads_) : nullptr) {}

AgentLayer::~AgentLayer() = default;

const char* AgentLayer::layerName() const {
    return kName;
}

void AgentLayer::onAttach(IWorld&) {
}

void AgentLayer::tick(IWorld& world, float dt) {
    if (batched_decision_hook_ && agents_.size() > 1) {
        tickBatched(world, dt);
        return;
    }
    if (decision_executor_ != nullptr && agents_.size() > 1) {
        decision_executor_->parallelFor(agents_.size(), [&](std::size_t index) {
            auto& agent = agents_[index];
            if (!agent->alive()) {
                return;
            }
            agent->beginTick();
            agent->decide(world, dt);
        });

        for (auto& agent : agents_) {
            if (!agent->alive()) {
                continue;
            }

            agent->publishDecisionEvent(world);
            agent->applyCapabilities(world, dt);
            agent->incrementAge(dt);

            const float previous_energy = agent->energy();
            float metabolism = base_metabolism_;
            if (agent->infected()) {
                metabolism *= 2.0f;
            }
            const float next_energy = previous_energy - metabolism * dt;
            agent->setEnergy(next_energy);
            if (next_energy != previous_energy) {
                world.eventBus().publish(events::Event{events::EventType::AgentEnergyChanged,
                                                       events::AgentEnergyChanged{agent->id(), previous_energy, next_energy},
                                                       world.currentTick(),
                                                       world.currentTimeSeconds()});
            }

            if (agent->energy() <= death_threshold_) {
                agent->kill("energy_depleted");
                world.eventBus().publish(events::Event{events::EventType::AgentDied,
                                                       events::AgentDied{agent->id(), "energy_depleted"},
                                                       world.currentTick(),
                                                       world.currentTimeSeconds()});
            }
        }
        return;
    }

    for (auto& agent : agents_) {
        if (!agent->alive()) {
            continue;
        }

        agent->beginTick();
        agent->decide(world, dt);
        agent->publishDecisionEvent(world);
        agent->applyCapabilities(world, dt);
        agent->incrementAge(dt);

        const float previous_energy = agent->energy();
        float metabolism = base_metabolism_;
        if (agent->infected()) {
            metabolism *= 2.0f;
        }
        const float next_energy = previous_energy - metabolism * dt;
        agent->setEnergy(next_energy);
        if (next_energy != previous_energy) {
            world.eventBus().publish(events::Event{events::EventType::AgentEnergyChanged,
                                                   events::AgentEnergyChanged{agent->id(), previous_energy, next_energy},
                                                   world.currentTick(),
                                                   world.currentTimeSeconds()});
        }

        if (agent->energy() <= death_threshold_) {
            agent->kill("energy_depleted");
            world.eventBus().publish(events::Event{events::EventType::AgentDied,
                                                   events::AgentDied{agent->id(), "energy_depleted"},
                                                   world.currentTick(),
                                                   world.currentTimeSeconds()});
        }
    }
}

int AgentLayer::tickOrder() const {
    return 100;
}

void AgentLayer::addAgent(std::unique_ptr<Agent> agent, IWorld& world) {
    world.eventBus().publish(events::Event{events::EventType::AgentBorn,
                                           events::AgentBorn{agent->id(), agent->genomeId(), agent->position()},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
    agents_.push_back(std::move(agent));
}

void AgentLayer::clear() {
    agents_.clear();
}

std::size_t AgentLayer::size() const {
    return agents_.size();
}

std::size_t AgentLayer::livingCount() const {
    std::size_t count = 0;
    for (const auto& agent : agents_) {
        count += agent->alive() ? 1u : 0u;
    }
    return count;
}

std::vector<Agent*> AgentLayer::agents() {
    std::vector<Agent*> result;
    result.reserve(agents_.size());
    for (auto& agent : agents_) {
        result.push_back(agent.get());
    }
    return result;
}

std::vector<const Agent*> AgentLayer::agents() const {
    std::vector<const Agent*> result;
    result.reserve(agents_.size());
    for (const auto& agent : agents_) {
        result.push_back(agent.get());
    }
    return result;
}

void AgentLayer::setBatchedDecisionHook(BatchedDecisionHook hook) {
    batched_decision_hook_ = std::move(hook);
}

bool AgentLayer::hasBatchedDecisionHook() const {
    return static_cast<bool>(batched_decision_hook_);
}

void AgentLayer::tickBatched(IWorld& world, float dt) {
    // Phase 1 (parallel): perception + brain.prepareInputs (or stash for fallback).
    std::vector<std::uint8_t> wants_batch(agents_.size(), 0u);
    if (decision_executor_ != nullptr) {
        decision_executor_->parallelFor(agents_.size(), [&](std::size_t index) {
            auto& agent = agents_[index];
            if (!agent->alive()) return;
            agent->beginTick();
            wants_batch[index] = agent->decideUntilDense(world, dt) ? 1u : 0u;
        });
    } else {
        for (std::size_t i = 0; i < agents_.size(); ++i) {
            auto& agent = agents_[i];
            if (!agent->alive()) continue;
            agent->beginTick();
            wants_batch[i] = agent->decideUntilDense(world, dt) ? 1u : 0u;
        }
    }

    // Phase 2 (single batched call): hook folds dense matmul for batched_agents.
    std::vector<Agent*> batched_agents;
    batched_agents.reserve(agents_.size());
    for (std::size_t i = 0; i < agents_.size(); ++i) {
        if (wants_batch[i] != 0u) batched_agents.push_back(agents_[i].get());
    }
    if (!batched_agents.empty()) {
        batched_decision_hook_(world, dt, batched_agents);
    }

    // Phase 3 (parallel): finalize / write outputs.
    if (decision_executor_ != nullptr) {
        decision_executor_->parallelFor(agents_.size(), [&](std::size_t index) {
            auto& agent = agents_[index];
            if (!agent->alive()) return;
            agent->decideAfterDense(world, dt);
        });
    } else {
        for (auto& agent : agents_) {
            if (!agent->alive()) continue;
            agent->decideAfterDense(world, dt);
        }
    }

    // Phase 4 (serial): publish + capabilities + metabolism + death.
    for (auto& agent : agents_) {
        if (!agent->alive()) continue;

        agent->publishDecisionEvent(world);
        agent->applyCapabilities(world, dt);
        agent->incrementAge(dt);

        const float previous_energy = agent->energy();
        float metabolism = base_metabolism_;
        if (agent->infected()) {
            metabolism *= 2.0f;
        }
        const float next_energy = previous_energy - metabolism * dt;
        agent->setEnergy(next_energy);
        if (next_energy != previous_energy) {
            world.eventBus().publish(events::Event{events::EventType::AgentEnergyChanged,
                                                   events::AgentEnergyChanged{agent->id(), previous_energy, next_energy},
                                                   world.currentTick(),
                                                   world.currentTimeSeconds()});
        }

        if (agent->energy() <= death_threshold_) {
            agent->kill("energy_depleted");
            world.eventBus().publish(events::Event{events::EventType::AgentDied,
                                                   events::AgentDied{agent->id(), "energy_depleted"},
                                                   world.currentTick(),
                                                   world.currentTimeSeconds()});
        }
    }
}

}  // namespace neuro
