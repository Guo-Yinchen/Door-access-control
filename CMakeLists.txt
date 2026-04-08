cmake_minimum_required(VERSION 3.16)
project(Door_access_control LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_GPIO "Enable GPIO support" ON)

find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio objdetect face)
find_package(Threads REQUIRED)

add_executable(door_access_control
  src/main.cpp
  src/EVENT-BUS/event-bus.cpp
  src/verifier/verifier.cpp
  src/Risk/risk-policy.cpp
  src/Camera/camera-stream.cpp
  src/FACE/face-verifier.cpp
)

if(ENABLE_GPIO)
  find_path(GPIOD_INCLUDE_DIR gpiod.h)
  find_library(GPIOD_LIB gpiod)

  find_path(PIGPIO_INCLUDE_DIR pigpio.h)
  find_library(PIGPIO_LIB pigpio)

  if(NOT GPIOD_INCLUDE_DIR OR NOT GPIOD_LIB)
    message(FATAL_ERROR "GPIO support requested, but libgpiod was not found.")
  endif()

  if(NOT PIGPIO_INCLUDE_DIR OR NOT PIGPIO_LIB)
    message(FATAL_ERROR "GPIO support requested, but pigpio was not found.")
  endif()

  target_sources(door_access_control PRIVATE
    src/GPIO/gpio-line.cpp
    src/LED/led-logic-v1.cpp
    src/Magnetic-reader/Magnetic-reader.cpp
    src/BUZZER/buzzer.cpp
    src/SERVO/servo-lock.cpp
  )

  target_include_directories(door_access_control PRIVATE
    ${GPIOD_INCLUDE_DIR}
    ${PIGPIO_INCLUDE_DIR}
  )

  target_link_libraries(door_access_control PRIVATE
    ${GPIOD_LIB}
    ${PIGPIO_LIB}
    Threads::Threads
    rt
  )

  target_compile_definitions(door_access_control PRIVATE ENABLE_GPIO=1)
else()
  target_compile_definitions(door_access_control PRIVATE ENABLE_GPIO=0)
endif()

target_include_directories(door_access_control PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(door_access_control PRIVATE ${OpenCV_LIBS})