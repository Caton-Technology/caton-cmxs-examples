###################
#
# For generating makefile, you need provide the following parameters if they are not at the standard path:
#   EXAMPLE_INC_DIR_CMXS, EXAMPLE_INC_DIR_VLC, EXAMPLE_LIB_DIR_CMXS, EXAMPLE_LIB_DIR_VLC.
#   If you provide EXAMPLE_OUTPUT_DIR, it will generate output to this dir, else it generate output to cmake root dir.
#
# e.g.
# On linux based OS, you can generate makefile as:
# cmake -S . -B /path/to/out \
#    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
#    [-DEXAMPLE_INC_DIR_VLC=/path/to/vlc/includ/plugins] \
#    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib] \
#    [-DEXAMPLE_LIB_DIR_VLC=/path/to/vlc/lib]
#    [-DEXAMPLE_OUTPUT_DIR=/path/to/output] \
#
# On Windows, you can generate makefile as:
# cmake -G"NMake Makefiles" -S . -B /path/to/out \
#    [-DEXAMPLE_INC_DIR_CMXS=/path/to/cmxs/include] \
#    [-DEXAMPLE_INC_DIR_VLC=/path/to/vlc/includ/plugins] \
#    [-DEXAMPLE_LIB_DIR_CMXS=/path/to/cmxs/lib] \
#    [-DEXAMPLE_LIB_DIR_VLC=/path/to/vlc/lib]
#    [-DEXAMPLE_OUTPUT_DIR=/path/to/output] \
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.2)
PROJECT(access_cmxs_plugin)

if (CMAKE_HOST_WIN32)
  set(MODULE_NAME libaccess_cmxs_plugin)
else ()
  set(MODULE_NAME access_cmxs_plugin)
endif ()

if (NOT DEFINED EXAMPLE_BUILD_TYPE)
  set(EXAMPLE_BUILD_TYPE debug)
endif()

if (EXAMPLE_BUILD_TYPE STREQUAL "release")
  list(APPEND CFLAGS -DNDEBUG)
endif ()

set(CMAKE_VERBOSE_MAKEFILE ON)

if (NOT CMAKE_HOST_WIN32)
  set(CMAKE_CXX_FLAGS -std=c++11)
endif ()

if (CMAKE_HOST_WIN32)
  set(CFLAGS -D_CRT_SECURE_NO_WARNINGS)
endif ()
add_compile_options(${CFLAGS})

if (DEFINED EXAMPLE_INC_DIR_CMXS)
  include_directories(${EXAMPLE_INC_DIR_CMXS})
endif ()
if (DEFINED EXAMPLE_INC_DIR_VLC)
  include_directories(${EXAMPLE_INC_DIR_VLC})
endif ()

if (DEFINED EXAMPLE_LIB_DIR_CMXS)
  link_directories(${EXAMPLE_LIB_DIR_CMXS})
endif ()
if (DEFINED EXAMPLE_LIB_DIR_VLC)
  link_directories(${EXAMPLE_LIB_DIR_VLC})
endif ()

if (DEFINED EXAMPLE_OUTPUT_DIR)
  set(LIBRARY_OUTPUT_PATH ${EXAMPLE_OUTPUT_DIR})
endif ()

if (CMAKE_HOST_WIN32)
  set(vlc_lib libvlccore)
else ()
  set(vlc_lib vlccore)
endif ()

link_libraries(cmxssdk ${vlc_lib})
add_library(${MODULE_NAME} SHARED ./access_cmxs_plugin.cpp)
