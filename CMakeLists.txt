cmake_minimum_required (VERSION 3.8.2)

project (ThreadSafe)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Werror")
set(CMAKE_CXX_COMPILER /usr/bin/g++-7)
set(CMAKE_CXX_STANDARD 17)

find_package(Boost REQUIRED)
find_package(GTest REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories("src/")
include_directories("test/")

FILE(GLOB_RECURSE TEST_SRCS test/*.cpp)

add_executable(thread_safe_test
    ${TEST_SRCS}
)

target_link_libraries(thread_safe_test
    gmock
    gtest
    pthread
    )
    
