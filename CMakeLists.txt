cmake_minimum_required(VERSION 3.16)
project(DoorAccessControl CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(door_access_control
  src/main.cpp
  src/LED/led-gpio-line.cpp
  src/LED/led-logic-v1.cpp
)

target_include_directories(door_access_control PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(door_access_control PRIVATE gpiod)
target_compile_options(door_access_control PRIVATE -Wall -Wextra -Wpedantic)
