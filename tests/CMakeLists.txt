cmake_minimum_required(VERSION 3.16)

# ── GoogleTest (FetchContent) ──────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# ── CardVerifier tests ─────────────────────────────────────────────────────────
add_executable(test_card_verifier
  test_card_verifier.cpp
  ${CMAKE_SOURCE_DIR}/../src/verifier/verifier.cpp
)
target_include_directories(test_card_verifier PRIVATE
  ${CMAKE_SOURCE_DIR}/../include
)
target_link_libraries(test_card_verifier PRIVATE GTest::gtest_main)
add_test(NAME CardVerifierTests COMMAND test_card_verifier)

# ── EventBus tests ─────────────────────────────────────────────────────────────
add_executable(test_event_bus
  test_event_bus.cpp
  ${CMAKE_SOURCE_DIR}/../src/EVENT-BUS/event-bus.cpp
)
target_include_directories(test_event_bus PRIVATE
  ${CMAKE_SOURCE_DIR}/../include
)
target_link_libraries(test_event_bus PRIVATE GTest::gtest_main)
add_test(NAME EventBusTests COMMAND test_event_bus)
