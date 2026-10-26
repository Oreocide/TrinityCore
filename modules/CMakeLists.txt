# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# AzerothCore-style out-of-tree modules.
#
# Every directory modules/<name>/ that contains a src/ subdirectory is a module.
#   modules/<name>/src/            sources, must define void Add<name_with_underscores>Scripts()
#   modules/<name>/conf/*.conf.dist installed into ${CONF_DIR}/worldserver.conf.d/
#   modules/<name>/data/sql/db-{auth,characters,world,hotfixes}/
#                                  applied automatically by the DB updater
#   modules/<name>/<name>.cmake    optional, included after the module sources were added
#                                  (use MODULE_TARGET to add dependencies)
#
# Each module gets a MODULE_<NAME> cache variable (static|disabled).

message("")

function(GetModuleList variable)
  file(GLOB CANDIDATES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)
  set(RESULT)
  foreach(CANDIDATE ${CANDIDATES})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${CANDIDATE}/src)
      list(APPEND RESULT ${CANDIDATE})
    endif()
  endforeach()
  list(SORT RESULT)
  set(${variable} ${RESULT} PARENT_SCOPE)
endfunction()

function(ModuleNameToIdentifier module variable)
  string(REGEX REPLACE "[^A-Za-z0-9_]" "_" IDENTIFIER ${module})
  set(${variable} ${IDENTIFIER} PARENT_SCOPE)
endfunction()

GetModuleList(MODULE_LIST)

add_library(modules STATIC)

set(MODULE_TARGET modules)
set(ENABLED_MODULES)
unset(TC_MODULES_FORWARD_DECL)
unset(TC_MODULES_INVOKE)
unset(TC_MODULES_NAMES)

message("* Module configuration:
    |")

foreach(MODULE ${MODULE_LIST})
  ModuleNameToIdentifier(${MODULE} MODULE_IDENTIFIER)
  string(TOUPPER "MODULE_${MODULE_IDENTIFIER}" MODULE_VARIABLE)
  set(${MODULE_VARIABLE} "static" CACHE STRING "Build type of the ${MODULE} module.")
  set_property(CACHE ${MODULE_VARIABLE} PROPERTY STRINGS static disabled)

  if(${MODULE_VARIABLE} STREQUAL "static")
    message("    +- ${MODULE}")
    set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE})
    list(APPEND ENABLED_MODULES ${MODULE})

    CollectAndAddSourceFiles(modules ${MODULE_PATH}/src HEADER_VISIBILITY PRIVATE)

    set(TC_MODULES_FORWARD_DECL "${TC_MODULES_FORWARD_DECL}void Add${MODULE_IDENTIFIER}Scripts();\n")
    set(TC_MODULES_INVOKE "${TC_MODULES_INVOKE}    Add${MODULE_IDENTIFIER}Scripts();\n")
    set(TC_MODULES_NAMES "${TC_MODULES_NAMES}        \"${MODULE}\",\n")

    file(GLOB MODULE_CONFIGS ${MODULE_PATH}/conf/*.conf.dist)
    if(MODULE_CONFIGS AND UNIX)
      install(FILES ${MODULE_CONFIGS} DESTINATION ${CONF_DIR}/worldserver.conf.d)
    elseif(MODULE_CONFIGS AND WIN32)
      install(FILES ${MODULE_CONFIGS} DESTINATION "${CMAKE_INSTALL_PREFIX}/worldserver.conf.d")
    endif()

    if(EXISTS ${MODULE_PATH}/${MODULE}.cmake)
      include(${MODULE_PATH}/${MODULE}.cmake)
    endif()
  elseif(${MODULE_VARIABLE} STREQUAL "disabled")
    message("    |  ${MODULE} (disabled)")
  else()
    message(FATAL_ERROR "Unknown value \"${${MODULE_VARIABLE}}\" for ${MODULE_VARIABLE}!")
  endif()
endforeach()

if(NOT ENABLED_MODULES)
  message("    |  (none)")
endif()
message("    |")

set(GENERATED_LOADER ${CMAKE_CURRENT_BINARY_DIR}/gen_moduleloader/ModulesLoader.cpp)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ModulesLoader.cpp.in.cmake ${GENERATED_LOADER})

target_sources(modules
  PRIVATE
    ${GENERATED_LOADER})

target_sources(modules
  PUBLIC
    FILE_SET HEADERS FILES ModulesScriptLoader.h)

target_link_libraries(modules
  PRIVATE
    trinity-core-interface
  PUBLIC
    game-interface)

set_target_properties(modules
  PROPERTIES
    COMPILE_WARNING_AS_ERROR ${WITH_WARNINGS_AS_ERRORS}
    FOLDER "server")

message("")
