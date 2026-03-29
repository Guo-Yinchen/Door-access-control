cmake_minimum_required(VERSION 3.16)
project(face_training LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs objdetect face)

add_executable(train_faces
  train_faces.cpp
)

target_include_directories(train_faces PRIVATE
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(train_faces PRIVATE
  ${OpenCV_LIBS}
)