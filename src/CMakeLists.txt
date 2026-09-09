cmake_minimum_required(VERSION 3.20)

project(FruityPrimeSourceTree LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

# The managed projects and the native C++ tree intentionally coexist under
# src while the migration is in progress.  This makes `src` the build entry
# point for the C++ replacement without deleting the managed reference.
add_subdirectory(MphRead.Native)
