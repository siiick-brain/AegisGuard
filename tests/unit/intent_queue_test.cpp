#include "aegisguard/intent_queue.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace aegisguard {
namespace {

TEST(IntentQueueTest, PreservesFifoOrderAndHoldsLastIntent) {
    IntentQueue queue(2);

    EXPECT_TRUE(queue.enqueue(Vec2{1.0, 0.0}));
    EXPECT_TRUE(queue.enqueue(Vec2{0.0, 1.0}));
    EXPECT_EQ(queue.pending_size(), 2U);

    EXPECT_DOUBLE_EQ(queue.advance_tick().x, 1.0);
    EXPECT_DOUBLE_EQ(queue.advance_tick().y, 1.0);
    EXPECT_DOUBLE_EQ(queue.advance_tick().y, 1.0);
    EXPECT_EQ(queue.pending_size(), 0U);
}

TEST(IntentQueueTest, RejectsEnqueueWhenBoundIsReached) {
    IntentQueue queue(1);

    EXPECT_TRUE(queue.enqueue(Vec2{1.0, 0.0}));
    EXPECT_FALSE(queue.enqueue(Vec2{0.0, 1.0}));
    EXPECT_EQ(queue.pending_size(), 1U);
}

TEST(IntentQueueTest, RejectsZeroCapacity) {
    EXPECT_THROW(IntentQueue queue(0), std::invalid_argument);
}

}  // namespace
}  // namespace aegisguard
