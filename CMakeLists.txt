cmake_minimum_required(VERSION 3.16)
project(Door_access_control LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio objdetect face)

add_executable(door_access_control
  src/main.cpp
  src/EVENT-BUS/event-bus.cpp
  src/GPIO/gpio-line.cpp
  src/LED/led-logic-v1.cpp
  src/Magnetic-reader/Magnetic-reader.cpp
  src/verifier/verifier.cpp
  src/Risk/risk-policy.cpp
  src/Camera/camera-stream.cpp
  src/FACE/face-verifier.cpp
  src/BUZZER/buzzer.cpp
  src/SERVO/servo-lock.cpp
)

target_include_directories(door_access_control PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${OpenCV_INCLUDE_DIRS}
)

find_library(GPIOD_LIB gpiod)

if (NOT GPIOD_LIB)
  message(WARNING "libgpiod not found. Building without GPIO support on this platform.")
endif()

target_link_libraries(door_access_control PRIVATE
  ${OpenCV_LIBS}
)

if (GPIOD_LIB)
  target_link_libraries(door_access_control PRIVATE
    ${GPIOD_LIB}
  )
endif()
