###################
#
# For generating makefile, you need provide the following parameters if they are not at the standard path:
#   EXAMPLE_INC_DIR_CMXS, EXAMPLE_LIB_DIR_CMXS.
#   If you provide EXAMPLE_OUTPUT_DIR, it will generate output to this dir, else it generate output to cmake root dir.
#   If you do a cross compiling, please provide cross compile options.
#   The example_cross_compile.cmake is an example cross compile file.
#
# e.g.
# On linux based OS, you can generate makefile as:
# cmake -S . -B /path/to/out \
#    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
#    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib] \
#    [-DEXAMPLE_OUTPUT_DIR=/path/to/output] \
#    [cross compile options]
#
# On Windows, you can generate makefile as:
# cmake -G"NMake Makefiles" -S . -B /path/to/out \
#    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
#    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib]
#    [-DEXAMPLE_OUTPUT_DIR=/path/to/output] \
#    [cross compile options]
#
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.2)
PROJECT(cmxs_cpp_example)

set(MODULE_NAME cmxs_cpp_example)

if (NOT DEFINED EXAMPLE_BUILD_TYPE)
  set(EXAMPLE_BUILD_TYPE debug)
endif()

if (EXAMPLE_BUILD_TYPE STREQUAL "release")
  list(APPEND CFLAGS -DNDEBUG)
endif ()
add_compile_options(${CFLAGS})

if (NOT CMAKE_HOST_WIN32)
  set(CMAKE_CXX_FLAGS -std=c++11)
endif ()

set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(../common)
if (DEFINED EXAMPLE_INC_DIR_CMXS)
  include_directories(${EXAMPLE_INC_DIR_CMXS})
endif ()

if (DEFINED EXAMPLE_LIB_DIR_CMXS)
  link_directories(${EXAMPLE_LIB_DIR_CMXS})
endif ()

set (SRC_FILES ./example_cpp.cpp)
if (CMAKE_HOST_WIN32)
  set(SRC_FILES ${SRC_FILES} ../common/getopt_win.c)
endif ()

if (DEFINED EXAMPLE_OUTPUT_DIR)
  set(EXECUTABLE_OUTPUT_PATH ${EXAMPLE_OUTPUT_DIR})
endif ()

link_libraries(cmxssdk)
add_executable(${MODULE_NAME} ${SRC_FILES})
