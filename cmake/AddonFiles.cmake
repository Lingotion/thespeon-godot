# This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
# The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

# The files that make up the addon, shared by PackageAddon.cmake and DeployAddon.cmake
# so the zip and a deployed copy never differ.
#
# thespeon_addon_files(<root> <out_var>) sets <out_var> to repo-relative paths:
# lingotion-thespeon.gdextension, every runtime library in bin/, and whatever
# release-files.txt lists. Each keeps its relative path inside the addon.

set(THESPEON_ADDON_DIR "addons/lingotion-thespeon")

function(thespeon_addon_files root out_var)
  # Runtime libraries only: the .exp/.lib/.pdb beside them are link-time and debug
  # artifacts that a Godot project never loads.
  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(_lib_glob "bin/*.dll")
  elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(_lib_glob "bin/*.dylib")
  else()
    set(_lib_glob "bin/*.so*")
  endif()

  set(_files "lingotion-thespeon.gdextension")

  file(GLOB _libs RELATIVE "${root}" "${root}/${_lib_glob}")
  if(NOT _libs)
    message(FATAL_ERROR
      "No runtime libraries in bin/ (looked for ${_lib_glob}). Build first:\n"
      "  cmake --build build --config Debug --target thespeon_gdextension")
  endif()
  list(APPEND _files ${_libs})

  # The vendor libraries and the extension share a directory and an extension, so a
  # glob alone cannot tell "built the extension" from "staged the vendor libs and
  # nothing else". Without this check a missing extension ships and simply fails to load.
  set(_have_extension FALSE)
  foreach(_lib IN LISTS _libs)
    if(_lib MATCHES "template_")
      set(_have_extension TRUE)
    endif()
  endforeach()
  if(NOT _have_extension)
    string(REPLACE ";" "\n  " _found "${_libs}")
    message(FATAL_ERROR
      "bin/ has no GDExtension library (nothing matching 'template_'). Found only:\n"
      "  ${_found}\n"
      "Build the extension first:\n"
      "  cmake --build build --target thespeon_gdextension")
  endif()

  # release-files.txt: one repo-relative path per line, # comments and blanks ignored.
  set(_manifest "${root}/release-files.txt")
  if(EXISTS "${_manifest}")
    file(STRINGS "${_manifest}" _lines)
    foreach(_line IN LISTS _lines)
      string(STRIP "${_line}" _line)
      if(_line STREQUAL "" OR _line MATCHES "^#")
        continue()
      endif()
      if(NOT EXISTS "${root}/${_line}")
        message(FATAL_ERROR "release-files.txt lists '${_line}', which does not exist.")
      endif()
      list(APPEND _files "${_line}")
    endforeach()
  endif()

  list(REMOVE_DUPLICATES _files)
  set(${out_var} "${_files}" PARENT_SCOPE)
endfunction()
