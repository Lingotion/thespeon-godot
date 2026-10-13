# This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
# The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

cmake_minimum_required(VERSION 3.23)
project(thespeon_gdextension CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Always the RELEASE dynamic CRT, even in Debug: Godot and the vendor thespeon.dll
# use it, the debug CRT (ucrtbased.dll) is not redistributable, and mixing the two
# shows up as a silent load failure on machines without Visual Studio.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

# --- vendor C library ---------------------------------------------------------
# Builds third_party/libthespeon and stages thespeon + onnxruntime into bin/ and lib/.

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ThespeonSDK)

# --- godot-cpp ----------------------------------------------------------------
# godot-cpp has no 4.7 branch; 4.4 matches compatibility_minimum in the
# .gdextension and GDExtension is forward-compatible with newer engines.

include(FetchContent)

set(GODOTCPP_TARGET "editor" CACHE STRING "godot-cpp build profile")

FetchContent_Declare(godot-cpp
  GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
  GIT_TAG        4.4
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(godot-cpp)

# --- the extension ------------------------------------------------------------

add_library(thespeon_gdextension SHARED
  src/register_types.cpp
  src/thespeon_audio_stream.cpp
  src/thespeon_catalog.cpp
  src/thespeon_importer.cpp
  src/thespeon_input.cpp
  src/thespeon_node.cpp
)

target_link_libraries(thespeon_gdextension PRIVATE godot-cpp thespeon_sdk)

# --- editor documentation -----------------------------------------------------
# The class reference in doc_classes/ is compiled into the extension. Regenerate the
# skeletons after changing the bindings by running, from a project with the addon:
#   godot --headless --doctool <this repo> --gdextension-docs
# godot-cpp's target_doc_sources expands the file list unquoted and so keeps only
# the first file; generate_doc_source is called directly instead. It runs
# Python3_EXECUTABLE, which godot-cpp finds only in its own directory scope.

find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB THESPEON_DOC_XML CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/doc_classes/*.xml")
set(_thespeon_doc_source "${CMAKE_CURRENT_BINARY_DIR}/gen/doc_source.cpp")
generate_doc_source("${_thespeon_doc_source}" "${THESPEON_DOC_XML}")
target_sources(thespeon_gdextension PRIVATE "${_thespeon_doc_source}")
add_dependencies(thespeon_gdextension generate_doc_source)

# thespeon_sdk is IMPORTED, so it carries no build ordering of its own: without this
# the link can start before libthespeon has produced the import library.
add_dependencies(thespeon_gdextension libthespeon_ext)

if(WIN32)
  set(_thespeon_tag "windows.template_debug.x86_64")
elseif(APPLE)
  set(_thespeon_tag "macos.template_debug")
else()
  set(_thespeon_tag "linux.template_debug.x86_64")
endif()

# A SHARED library is a RUNTIME artifact on Windows but a LIBRARY one everywhere else,
# so both have to point at bin/ or the macOS/Linux build lands in the build tree.
set_target_properties(thespeon_gdextension PROPERTIES
  PREFIX ""
  OUTPUT_NAME "thespeon.${_thespeon_tag}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

# Multi-config generators (Visual Studio) otherwise append Debug/ or Release/.
foreach(_config Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${_config}" _CONFIG)
  set_target_properties(thespeon_gdextension PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_${_CONFIG} "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY_${_CONFIG} "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY_${_CONFIG} "${CMAKE_CURRENT_SOURCE_DIR}/bin"
  )
endforeach()

# --- deploying into a Godot project -------------------------------------------
# With -DGODOT_PROJECT_DIR=path/to/project, every build of the extension also copies
# the addon into that project's addons/lingotion-thespeon/.

set(GODOT_PROJECT_DIR "" CACHE PATH
  "Godot project to copy the addon into after each build; empty to skip.")

if(NOT GODOT_PROJECT_DIR STREQUAL "")
  add_custom_command(TARGET thespeon_gdextension POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DGODOT_PROJECT_DIR=${GODOT_PROJECT_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployAddon.cmake"
    COMMENT "Deploying the addon into ${GODOT_PROJECT_DIR}"
    VERBATIM)
endif()

# --- packaging ----------------------------------------------------------------

add_custom_target(package_addon
  COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackageAddon.cmake"
  DEPENDS thespeon_gdextension
  COMMENT "Packaging the addon zip"
  VERBATIM)
