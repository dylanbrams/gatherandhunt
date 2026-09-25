cmake_minimum_required(VERSION 3.20)
project(TurtleSim CXX)

# UnrealBuildTool compiles every .cpp under this directory into the UE TurtleSim
# module, CMake's own compiler-probe sources included, which then collide over
# main() at link time. So the build tree has to sit outside the module.
get_filename_component(_turtlesim_src "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
get_filename_component(_turtlesim_bin "${CMAKE_BINARY_DIR}" REALPATH)
string(FIND "${_turtlesim_bin}" "${_turtlesim_src}" _turtlesim_bin_inside_src)
if(_turtlesim_bin_inside_src EQUAL 0)
    message(FATAL_ERROR
        "The build directory must not be inside Source/TurtleSim -- UnrealBuildTool "
        "would compile CMake's scratch sources into the UE module and fail to link.\n"
        "Configure into the project's Intermediate folder instead, e.g.\n"
        "  cmake -S . -B ../../Intermediate/TurtleSimCMake")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /WX /Zc:preprocessor)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

set(TURTLESIM_SOURCES
    Private/Orientation.cpp
    Private/BoardState.cpp
    Private/CostModel.cpp
    Private/RandomPCG.cpp
    Private/Validation.cpp
    Private/HunterAI.cpp
    Private/Resolver.cpp
    Private/BoardGenerator.cpp
    Private/PlanPreview.cpp
)

set(TURTLESIM_HEADERS
    Public/IntVec3.h
    Public/Constants.h
    Public/Orientation.h
    Public/Move.h
    Public/BoardState.h
    Public/CostModel.h
    Public/RandomPCG.h
    Public/Validation.h
    Public/Events.h
    Public/HunterAI.h
    Public/Resolver.h
    Public/BoardGenerator.h
    Public/PlanPreview.h
)

add_library(turtlesim STATIC ${TURTLESIM_SOURCES} ${TURTLESIM_HEADERS})
target_include_directories(turtlesim PUBLIC Public)

# Test runner.
enable_testing()
add_executable(turtlesim_tests
    Tests/TestMain.cpp
    Tests/OrientationTests.cpp
    Tests/BoardStateTests.cpp
    Tests/CostModelTests.cpp
    Tests/RandomPCGTests.cpp
    Tests/ValidationTests.cpp
    Tests/HunterAITests.cpp
    Tests/ResolverCollisionTests.cpp
    Tests/BoardGeneratorTests.cpp
    Tests/DeterminismFuzzTests.cpp
    Tests/PlanPreviewTests.cpp
)
target_link_libraries(turtlesim_tests PRIVATE turtlesim)
target_include_directories(turtlesim_tests PRIVATE Tests)
add_test(NAME turtlesim_tests COMMAND turtlesim_tests)
