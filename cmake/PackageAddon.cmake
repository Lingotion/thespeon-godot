# This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
# The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

# Script mode (cmake -P). Zips the built addon so it extracts straight into the root
# of a Godot project.
#
#   cmake -P cmake/PackageAddon.cmake
#   cmake -P cmake/PackageAddon.cmake -DVERSION=0.1.0
#
# Contents: lingotion-thespeon.gdextension, every runtime library in bin/, and whatever
# release-files.txt lists. Everything sits under addons/lingotion-thespeon/ inside the
# zip, matching the res:// paths in the manifest, so unzipping at the project root
# puts every file exactly where Godot looks for it.
#
# Optional: VERSION (tag for the archive name), OUT_DIR (default dist/).

cmake_minimum_required(VERSION 3.23)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

include("${CMAKE_CURRENT_LIST_DIR}/AddonFiles.cmake")
set(ADDON_DIR "${THESPEON_ADDON_DIR}")

if(NOT DEFINED OUT_DIR)
  set(OUT_DIR "${ROOT}/dist")
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
  set(_platform "windows-x86_64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  set(_platform "macos")
else()
  set(_platform "linux-x86_64")
endif()

thespeon_addon_files("${ROOT}" _files)

# --- stage --------------------------------------------------------------------

set(_stage "${OUT_DIR}/stage")

file(REMOVE_RECURSE "${_stage}")
file(MAKE_DIRECTORY "${_stage}/${ADDON_DIR}")

foreach(_file IN LISTS _files)
  get_filename_component(_subdir "${_file}" DIRECTORY)
  file(COPY "${ROOT}/${_file}"
    DESTINATION "${_stage}/${ADDON_DIR}/${_subdir}"
    FOLLOW_SYMLINK_CHAIN)
  message(STATUS "packaging ${ADDON_DIR}/${_file}")
endforeach()

# --- archive ------------------------------------------------------------------

if(DEFINED VERSION AND NOT VERSION STREQUAL "")
  set(_name "lingotion-thespeon-${VERSION}-${_platform}.zip")
else()
  set(_name "lingotion-thespeon-${_platform}.zip")
endif()

set(_archive "${OUT_DIR}/${_name}")
file(REMOVE "${_archive}")

# `cmake -E tar` rather than file(ARCHIVE_CREATE): it takes a WORKING_DIRECTORY, which
# is what keeps the paths inside the zip relative to the project root instead of absolute.
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar cf "${_archive}" --format=zip -- addons
  WORKING_DIRECTORY "${_stage}"
  RESULT_VARIABLE _result)

if(NOT _result EQUAL 0)
  message(FATAL_ERROR "Creating ${_archive} failed.")
endif()

file(REMOVE_RECURSE "${_stage}")
message(STATUS "wrote ${_archive}")
