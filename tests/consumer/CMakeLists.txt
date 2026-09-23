cmake_minimum_required(VERSION 3.25)
project(AveMotionInstalledConsumer LANGUAGES CXX)

find_package(AveMotion 0.24.0 REQUIRED CONFIG)

add_executable(avemotion_installed_consumer main.cpp)
target_compile_features(avemotion_installed_consumer PRIVATE cxx_std_20)
target_link_libraries(avemotion_installed_consumer PRIVATE
    AveMotion::Formats
    AveMotion::Core
    AveMotion::Model
    AveMotion::Validation
    AveMotion::Evaluation
    AveMotion::Runtime
    AveMotion::Player
    AveMotion::Rendering)
