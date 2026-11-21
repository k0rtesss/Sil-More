set(_sil_version_parts)
foreach(_component MAJOR MINOR PATCH)
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/defines.h" _definition
    REGEX "^#define[ \t]+VERSION_${_component}[ \t]+[0-9]+$")
  if(NOT _definition)
    message(FATAL_ERROR "Missing numeric VERSION_${_component} in src/defines.h")
  endif()
  string(REGEX MATCH "[0-9]+$" _number "${_definition}")
  list(APPEND _sil_version_parts "${_number}")
endforeach()
list(JOIN _sil_version_parts "." _sil_runtime_version)
if(NOT SIL_VERSION_STRING STREQUAL _sil_runtime_version)
  message(FATAL_ERROR "VERSION_STRING (${SIL_VERSION_STRING}) must match the runtime version (${_sil_runtime_version})")
endif()

# Keep the normal init_info consumers in sync without duplicating their list.
# Runtime templates that bypass the raw cache and call init_info_txt directly.
set(_sil_template_loader "${CMAKE_CURRENT_SOURCE_DIR}/src/init/init-info.c")
file(READ "${_sil_template_loader}" _sil_template_loader_text)
string(REGEX MATCHALL "init_info\\(\"[a-z_-]+\""
  _sil_template_calls "${_sil_template_loader_text}")
set(_sil_template_names style-levels partition set ability-insight)
foreach(_sil_template_call IN LISTS _sil_template_calls)
  string(REGEX REPLACE "init_info\\(\"([a-z_-]+)\"" "\\1"
    _sil_template_name "${_sil_template_call}")
  list(APPEND _sil_template_names "${_sil_template_name}")
endforeach()
list(REMOVE_DUPLICATES _sil_template_names)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${_sil_template_loader}")
foreach(_sil_template_name IN LISTS _sil_template_names)
  set(_sil_template_path
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/edit/${_sil_template_name}.txt")
  if(NOT EXISTS "${_sil_template_path}")
    message(FATAL_ERROR "Missing runtime template: ${_sil_template_path}")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_sil_template_path}")
  file(STRINGS "${_sil_template_path}" _sil_template_version REGEX "^V:")
  if(NOT "${_sil_template_version}" STREQUAL "V:${SIL_VERSION_STRING}")
    message(FATAL_ERROR
      "${_sil_template_name}.txt has '${_sil_template_version}', expected V:${SIL_VERSION_STRING}. Update the runtime template headers with the game version before building.")
  endif()
endforeach()
