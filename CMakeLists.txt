cmake_minimum_required(VERSION 3.16)
project(Door_access_control LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(door_access_control
  src/main.cpp
  src/EVENT-BUS/event-bus.cpp
  src/GPIO/gpio-line.cpp
  src/LED/led-logic-v1.cpp
  src/Magnetic-reader/Magnetic-reader.cpp
  src/verifier/verifier.cpp
)

target_include_directories(door_access_control PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

find_library(GPIOD_LIB gpiod)
if (NOT GPIOD_LIB)
  message(FATAL_ERROR "libgpiod not found. Install it with: sudo apt install libgpiod-dev")
endif()
target_link_libraries(door_access_control PRIVATE ${GPIOD_LIB})