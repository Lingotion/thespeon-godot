# This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
# The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

# Script mode (cmake -P). Copies the libthespeon sub-build's artifacts into this
# repo's bin/ and lib/, which is where lingotion-thespeon.gdextension expects to find them.
#
# libthespeon defines no install() rules, so there is nothing to `cmake --install`;
# its C ABI library is written to the build root by src/abi/CMakeLists.txt and ONNX
# Runtime is dropped beside it by metagraph_stage_runtime().
#
# Expects: BUILD_DIR CONFIG STAGE_BIN STAGE_LIB RUNTIME_GLOB IMPLIB_GLOB ONNXRUNTIME_GLOB

# Multi-config generators (Visual Studio) add the per-config directory; single-config
# ones write straight to the build root.
set(_search_dirs "${BUILD_DIR}/${CONFIG}" "${BUILD_DIR}")

function(_stage_artifact glob destination description)
  if(glob STREQUAL "")
    return()
  endif()

  set(_matches "")
  foreach(_dir IN LISTS _search_dirs)
    file(GLOB _hits "${_dir}/${glob}")
    if(_hits)
      set(_matches "${_hits}")
      break()
    endif()
  endforeach()

  if(NOT _matches)
    string(REPLACE ";" "\n  " _looked "${_search_dirs}")
    message(FATAL_ERROR
      "Could not find the ${description} ('${glob}') after building libthespeon.\n"
      "Looked in:\n  ${_looked}")
  endif()

  file(MAKE_DIRECTORY "${destination}")
  # FOLLOW_SYMLINK_CHAIN keeps the ELF/Mach-O version chain intact: the linker sees
  # libthespeon.so while the recorded SONAME resolves to libthespeon.so.1 next to it.
  file(COPY ${_matches} DESTINATION "${destination}" FOLLOW_SYMLINK_CHAIN)

  foreach(_match IN LISTS _matches)
    get_filename_component(_name "${_match}" NAME)
    message(STATUS "staged ${_name} -> ${destination}")
  endforeach()
endfunction()

_stage_artifact("${RUNTIME_GLOB}"      "${STAGE_BIN}" "thespeon runtime library")
_stage_artifact("${ONNXRUNTIME_GLOB}"  "${STAGE_BIN}" "ONNX Runtime shared library")
_stage_artifact("${IMPLIB_GLOB}"       "${STAGE_LIB}" "thespeon import library")
