#include <algorithm>
#include <functional>
#include <queue>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include <subordination/base/queue_popper.hh>
#include <subordination/base/queue_pusher.hh>

#define MAKE_QUEUE_PUSHER_TEST(Queue, Pusher) \
    std::default_random_engine rng; \
    Queue queue; \
    std::generate_n(Pusher(queue), 100, std::ref(rng)); \
    EXPECT_EQ(100u, queue.size());

#define MAKE_QUEUE_POPPER_TEST(Queue, Popper) \
    std::vector<T> result; \
    std::copy( \
        Popper(queue), \
        Popper##_end(queue), \
        std::back_inserter(result) \
    ); \
    EXPECT_EQ(100u, result.size()); \
    EXPECT_EQ(0u, queue.size());

typedef std::default_random_engine::result_type T;

TEST(QueuePusherAndPopper, Queue) {
    MAKE_QUEUE_PUSHER_TEST(std::queue<T>, sbn::queue_pusher);
    MAKE_QUEUE_POPPER_TEST(std::queue<T>, sbn::queue_popper);
}

TEST(QueuePusherAndPopper, PriorityQueue) {
    MAKE_QUEUE_PUSHER_TEST(std::priority_queue<T>, sbn::priority_queue_pusher);
    MAKE_QUEUE_POPPER_TEST(std::priority_queue<T>, sbn::priority_queue_popper);
}

TEST(QueuePusherAndPopper, Deque) {
    MAKE_QUEUE_PUSHER_TEST(std::deque<T>, sbn::deque_pusher);
    MAKE_QUEUE_POPPER_TEST(std::deque<T>, sbn::deque_popper);
}
