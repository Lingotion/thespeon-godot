# This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
# The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

# Script mode (cmake -P). Copies the built addon into a Godot project, as if the
# package zip had been extracted at its root. Runs after every extension build when
# GODOT_PROJECT_DIR is set; see the top-level CMakeLists.
#
#   cmake -P cmake/DeployAddon.cmake -DGODOT_PROJECT_DIR=path/to/project
#
# Only the addon's own files are written, so runtime-files/ (imported models) and
# Godot's .uid/.import files beside them are left alone.

cmake_minimum_required(VERSION 3.23)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/AddonFiles.cmake")

if(NOT DEFINED GODOT_PROJECT_DIR OR GODOT_PROJECT_DIR STREQUAL "")
  message(FATAL_ERROR "Pass -DGODOT_PROJECT_DIR=<path to a Godot project>.")
endif()
get_filename_component(_project "${GODOT_PROJECT_DIR}" ABSOLUTE BASE_DIR "${ROOT}")
if(NOT EXISTS "${_project}/project.godot")
  message(FATAL_ERROR "'${_project}' has no project.godot, so it is not a Godot project.")
endif()

thespeon_addon_files("${ROOT}" _files)

set(_addon "${_project}/${THESPEON_ADDON_DIR}")
set(_locked "")

foreach(_file IN LISTS _files)
  get_filename_component(_subdir "${_file}" DIRECTORY)
  file(MAKE_DIRECTORY "${_addon}/${_subdir}")
  # ONLY_IF_DIFFERENT leaves unchanged files alone, which also means a library the
  # editor holds open is only a problem when it actually changed.
  file(COPY_FILE "${ROOT}/${_file}" "${_addon}/${_file}"
    ONLY_IF_DIFFERENT RESULT _result)
  if(_result EQUAL 0)
    message(STATUS "deployed ${THESPEON_ADDON_DIR}/${_file}")
  else()
    list(APPEND _locked "${_file}")
  endif()
endforeach()

if(_locked)
  string(REPLACE ";" "\n  " _names "${_locked}")
  # Windows will not overwrite a DLL that a process has loaded.
  message(FATAL_ERROR
    "Could not copy into ${_addon}:\n  ${_names}\n"
    "If the Godot editor or game has this project open, close it and build again: "
    "the extension is only loaded at startup, so it cannot be replaced while running.")
endif()
