cmake_minimum_required(VERSION 3.16...3.28)

project(neuroframegen-demo LANGUAGES CXX C)

# TODO: Maybe remove
set(CMAKE_PREFIX_PATH "$ENV{CONDA_PREFIX}/lib/cmake" CACHE PATH "")
set(CMAKE_LIBRARY_PATH "$ENV{CONDA_PREFIX}/lib" CACHE PATH "")
link_directories("$ENV{CONDA_PREFIX}/lib")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
else()
    message(STATUS "Feel free to sudo apt install ccache if you want faster compilation times.")
endif()

add_executable(main src/main.cpp)

# Find installed dv-processing.
ADD_SUBDIRECTORY(include/dv-processing EXCLUDE_FROM_ALL)

# link your targets against the library
target_link_libraries(main
    PRIVATE
    dv::processing
)

if (MSVC)
    target_compile_options(main PRIVATE /W4 /WX)
else()
    target_compile_options(main PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -pedantic -Werror -Wno-error=unused-parameter -Wno-error=unused-but-set-variable>)
endif()

