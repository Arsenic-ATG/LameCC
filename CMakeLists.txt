cmake_minimum_required(VERSION 3.8)

project("LameCC")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Debug)
include_directories("third-party/json/single_include")
include_directories("third-party/ProgramOptions/include")
include_directories("third-party/rang/include")
aux_source_directory(./src DIR_SRCS)
add_executable(LameCC ${DIR_SRCS})

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set your project compile flags.
# E.g. if using the C++ header files
# you will need to enable C++11 support
# for your compiler.

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs core)

target_link_libraries(LameCC
    PRIVATE
        ${llvm_libs}
)