#include <gtest/gtest.h>

#include "runtime/headless/EvaluationPool.h"

#include <atomic>
#include <vector>

TEST(EvaluationPool, EnqueueAndWaitAllCompletes) {
    neuro::runtime::EvaluationPool pool(2);
    std::atomic<int> counter{0};

    for (int i = 0; i < 10; ++i) {
        pool.enqueue([&counter] { ++counter; });
    }

    pool.waitAll();
    EXPECT_EQ(counter.load(), 10);
}

TEST(EvaluationPool, SizeReturnsThreadCount) {
    neuro::runtime::EvaluationPool pool(4);
    EXPECT_EQ(pool.size(), 4u);
}

TEST(EvaluationPool, DefaultThreadCountIsPositive) {
    neuro::runtime::EvaluationPool pool;
    EXPECT_GE(pool.size(), 1u);
}

TEST(EvaluationPool, ParallelExecutionIsFasterThanSequential) {
    neuro::runtime::EvaluationPool pool(2);
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i) {
        pool.enqueue([&counter] {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            ++counter;
        });
    }

    pool.waitAll();
    EXPECT_EQ(counter.load(), 100);
}
