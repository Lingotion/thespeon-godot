# This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
# The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

# Builds libthespeon from third_party/ and exposes it as the `thespeon_sdk` IMPORTED
# target.
#
# It is built as a *separate* CMake project rather than with add_subdirectory():
# libthespeon's CMakeLists forces BUILD_SHARED_LIBS OFF, C++20 and hidden symbol
# visibility, none of which should leak into godot-cpp or into this extension.
#
# The sub-build is incremental, so only the first build is slow; later builds check
# and re-stage.

# libthespeon is the submodule third_party/libthespeon. Point LIBTHESPEON_DIR at
# another checkout to build against that instead; a relative path is taken from the
# repository root.
set(LIBTHESPEON_DIR "${CMAKE_SOURCE_DIR}/third_party/libthespeon"
  CACHE PATH "libthespeon source tree to build against.")
get_filename_component(_libthespeon_dir "${LIBTHESPEON_DIR}" ABSOLUTE
  BASE_DIR "${CMAKE_SOURCE_DIR}")

# Release even when this project is built Debug: libthespeon and Godot both use the
# release dynamic CRT, and mixing in the debug CRT shows up as a silent load failure.
# Same reasoning as CMAKE_MSVC_RUNTIME_LIBRARY in the top-level CMakeLists.
set(LIBTHESPEON_BUILD_TYPE "Release" CACHE STRING "Configuration to build libthespeon in.")

if(NOT EXISTS "${_libthespeon_dir}/CMakeLists.txt"
   OR NOT EXISTS "${_libthespeon_dir}/metagraph-cpp/CMakeLists.txt")
  message(FATAL_ERROR
    "No libthespeon source tree in ${_libthespeon_dir}. If it is the submodule, run "
    "this from the repository root:\n"
    "  git submodule update --init --recursive")
endif()

# --- what the sub-build produces, per platform --------------------------------
# src/abi/CMakeLists.txt sends the C ABI library to the build root, and
# metagraph_stage_runtime() copies ONNX Runtime beside it.

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(_thespeon_runtime_glob "thespeon.dll")
  set(_thespeon_implib_glob  "thespeon.lib")
  set(_onnxruntime_glob      "onnxruntime.dll")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(_thespeon_runtime_glob "libthespeon*.dylib")
  set(_thespeon_implib_glob  "")
  set(_onnxruntime_glob      "libonnxruntime*.dylib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(_thespeon_runtime_glob "libthespeon.so*")
  set(_thespeon_implib_glob  "")
  set(_onnxruntime_glob      "libonnxruntime.so*")
else()
  message(FATAL_ERROR
    "libthespeon supports Windows x64, Linux x86_64 and Apple Silicon macOS; "
    "CMAKE_SYSTEM_NAME='${CMAKE_SYSTEM_NAME}' is none of those.")
endif()

# Staging destinations. bin/ is also where the extension itself is written, so after
# a build it holds exactly the three libraries the .gdextension asks Godot to load.
set(THESPEON_STAGE_BIN "${CMAKE_SOURCE_DIR}/bin")
set(THESPEON_STAGE_LIB "${CMAKE_SOURCE_DIR}/lib")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(_thespeon_staged_runtime "${THESPEON_STAGE_BIN}/thespeon.dll")
  set(_thespeon_staged_implib  "${THESPEON_STAGE_LIB}/thespeon.lib")
  set(_thespeon_byproducts "${_thespeon_staged_runtime}" "${_thespeon_staged_implib}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(_thespeon_staged_runtime "${THESPEON_STAGE_BIN}/libthespeon.dylib")
  set(_thespeon_staged_implib  "")
  set(_thespeon_byproducts "${_thespeon_staged_runtime}")
else()
  # SOVERSION 1 / VERSION 1.0.0: the link input is the unversioned name, the recorded
  # SONAME is libthespeon.so.1, and the staging step copies the whole chain.
  set(_thespeon_staged_runtime "${THESPEON_STAGE_BIN}/libthespeon.so")
  set(_thespeon_staged_implib  "")
  set(_thespeon_byproducts "${_thespeon_staged_runtime}")
endif()

# --- build and stage ----------------------------------------------------------

include(ExternalProject)

# One sub-build per source tree: a CMake build directory is tied to its sources, so
# switching LIBTHESPEON_DIR must not reuse the other tree's.
get_filename_component(_libthespeon_name "${_libthespeon_dir}" NAME)
set(_libthespeon_build "${CMAKE_BINARY_DIR}/libthespeon-build-${_libthespeon_name}")

# A bare --parallel means an unlimited `make -j` with the Makefile generator, which
# runs out of memory on CI runners, so the job count is always spelled out. It follows
# CMAKE_BUILD_PARALLEL_LEVEL when that is set at configure time, like the outer build.
if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL} AND NOT "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}" STREQUAL "")
  set(_libthespeon_jobs "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
else()
  cmake_host_system_information(RESULT _libthespeon_jobs QUERY NUMBER_OF_LOGICAL_CORES)
endif()

ExternalProject_Add(libthespeon_ext
  SOURCE_DIR       "${_libthespeon_dir}"
  BINARY_DIR       "${_libthespeon_build}"
  # The submodule checkout is managed by git, not by ExternalProject.
  DOWNLOAD_COMMAND ""
  UPDATE_COMMAND   ""
  PATCH_COMMAND    ""
  CMAKE_ARGS
    "-DCMAKE_BUILD_TYPE=${LIBTHESPEON_BUILD_TYPE}"
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL"
    "-DTHESPEON_BUILD_C_ABI=ON"
    # The CLI and its miniaudio dependency are dead weight for a GDExtension.
    "-DTHESPEON_BUILD_CLI=OFF"
    # On by default when libthespeon is top level, which it is here; pulls GoogleTest.
    "-DTHESPEON_BUILD_TESTS=OFF"
  BUILD_COMMAND
    "${CMAKE_COMMAND}" --build "${_libthespeon_build}"
      --config "${LIBTHESPEON_BUILD_TYPE}" --parallel ${_libthespeon_jobs}
  # Re-run the (incremental) build every time, so a change in the submodule is picked
  # up instead of silently linking a stale library. Unchanged sources make this a no-op.
  BUILD_ALWAYS ON
  # libthespeon defines no install rules, so "install" means staging into bin/ and lib/.
  INSTALL_COMMAND
    "${CMAKE_COMMAND}"
      "-DBUILD_DIR=${_libthespeon_build}"
      "-DCONFIG=${LIBTHESPEON_BUILD_TYPE}"
      "-DSTAGE_BIN=${THESPEON_STAGE_BIN}"
      "-DSTAGE_LIB=${THESPEON_STAGE_LIB}"
      "-DRUNTIME_GLOB=${_thespeon_runtime_glob}"
      "-DIMPLIB_GLOB=${_thespeon_implib_glob}"
      "-DONNXRUNTIME_GLOB=${_onnxruntime_glob}"
      -P "${CMAKE_CURRENT_LIST_DIR}/StageThespeonSDK.cmake"
  BUILD_BYPRODUCTS ${_thespeon_byproducts}
  USES_TERMINAL_BUILD ON
)

# --- the imported target ------------------------------------------------------
# The staged files do not exist until libthespeon_ext has run. That is fine for an
# IMPORTED target: BUILD_BYPRODUCTS tells Ninja how they come to exist, and consumers
# take an add_dependencies() edge for the ordering.
#
# Includes point at the header in the submodule rather than a staged copy, so there is
# no second copy that can drift from the library actually being built.

add_library(thespeon_sdk SHARED IMPORTED GLOBAL)

set_target_properties(thespeon_sdk PROPERTIES
  IMPORTED_LOCATION             "${_thespeon_staged_runtime}"
  INTERFACE_INCLUDE_DIRECTORIES "${_libthespeon_dir}/src/abi")

if(_thespeon_staged_implib)
  set_target_properties(thespeon_sdk PROPERTIES
    IMPORTED_IMPLIB "${_thespeon_staged_implib}")
endif()
