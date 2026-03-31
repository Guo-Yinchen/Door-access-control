// tests/test_event_bus.cpp
//
// Unit tests for EventBus
//
// Covered cases:
//   1. Subscribe to LED → publish LED event → handler fires
//   2. Subscribe to LED → publish BUZZER-only event → handler does NOT fire
//   3. Two subscribers with different targets → only the matching one fires
//   4. Publish multiple events → handler receives them in the correct order
//
// Strategy: run dispatch_loop() in a background thread; after publishing all
// events call stop() then join() – exactly as described in the brief.

#include <gtest/gtest.h>
#include "EVENT/event-bus.hpp"
#include "AuthResult/AuthResult.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

// Helper: spin up dispatch_loop in a thread, return the thread
static std::thread start_bus(EventBus& bus) {
    return std::thread([&bus]() { bus.dispatch_loop(); });
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1: Subscribe LED – publish LED event – handler fires
// ─────────────────────────────────────────────────────────────────────────────
TEST(EventBusTest, SubscribeLed_PublishLed_HandlerFires) {
    EventBus bus;

    std::atomic<int> call_count{0};

    bus.subscribe(Target::LED, [&](const AuthEvent& e) {
        ++call_count;
    });

    auto t = start_bus(bus);

    bus.publish(AuthResult::granted, Target::LED);

    // Give the dispatch loop a moment to process
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bus.stop();
    t.join();

    EXPECT_EQ(call_count.load(), 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2: Subscribe LED – publish BUZZER-only event – handler does NOT fire
// ─────────────────────────────────────────────────────────────────────────────
TEST(EventBusTest, SubscribeLed_PublishBuzzerOnly_HandlerDoesNotFire) {
    EventBus bus;

    std::atomic<int> call_count{0};

    bus.subscribe(Target::LED, [&](const AuthEvent&) {
        ++call_count;
    });

    auto t = start_bus(bus);

    // Publish to BUZZER only – LED subscriber must NOT be triggered
    bus.publish(AuthResult::denied, static_cast<uint32_t>(Target::BUZZER));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bus.stop();
    t.join();

    EXPECT_EQ(call_count.load(), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3: Two subscribers with different targets – only the matching one fires
// ─────────────────────────────────────────────────────────────────────────────
TEST(EventBusTest, TwoSubscribersDifferentTargets_OnlyMatchingFires) {
    EventBus bus;

    std::atomic<int> led_count{0};
    std::atomic<int> buzzer_count{0};

    bus.subscribe(Target::LED, [&](const AuthEvent&) {
        ++led_count;
    });
    bus.subscribe(Target::BUZZER, [&](const AuthEvent&) {
        ++buzzer_count;
    });

    auto t = start_bus(bus);

    // Publish to LED only
    bus.publish(AuthResult::granted, static_cast<uint32_t>(Target::LED));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bus.stop();
    t.join();

    EXPECT_EQ(led_count.load(),    1);   // LED subscriber fired
    EXPECT_EQ(buzzer_count.load(), 0);   // BUZZER subscriber did NOT fire
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4: Publish multiple events – handler receives them in the correct order
// ─────────────────────────────────────────────────────────────────────────────
TEST(EventBusTest, MultipleEvents_ReceivedInOrder) {
    EventBus bus;

    std::mutex vec_mtx;
    std::vector<AuthResult> received;

    bus.subscribe(Target::LED, [&](const AuthEvent& e) {
        std::lock_guard<std::mutex> lk(vec_mtx);
        received.push_back(e.result);
    });

    auto t = start_bus(bus);

    // Publish three events in a defined order
    bus.publish(AuthResult::granted,      static_cast<uint32_t>(Target::LED));
    bus.publish(AuthResult::denied,       static_cast<uint32_t>(Target::LED));
    bus.publish(AuthResult::pending_face, static_cast<uint32_t>(Target::LED));

    // Wait long enough for all three to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    bus.stop();
    t.join();

    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[0], AuthResult::granted);
    EXPECT_EQ(received[1], AuthResult::denied);
    EXPECT_EQ(received[2], AuthResult::pending_face);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bonus: publish to ALL targets – every subscriber fires
// ─────────────────────────────────────────────────────────────────────────────
TEST(EventBusTest, PublishAll_AllSubscribersFire) {
    EventBus bus;

    std::atomic<int> led_count{0};
    std::atomic<int> buzzer_count{0};
    std::atomic<int> lock_count{0};

    bus.subscribe(Target::LED,    [&](const AuthEvent&) { ++led_count;    });
    bus.subscribe(Target::BUZZER, [&](const AuthEvent&) { ++buzzer_count; });
    bus.subscribe(Target::LOCK,   [&](const AuthEvent&) { ++lock_count;   });

    auto t = start_bus(bus);

    bus.publish(AuthResult::idle, Target::ALL);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bus.stop();
    t.join();

    EXPECT_EQ(led_count.load(),    1);
    EXPECT_EQ(buzzer_count.load(), 1);
    EXPECT_EQ(lock_count.load(),   1);
}
