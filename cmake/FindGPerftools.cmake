#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

#.rst:
# FindGPerftools.cmake
# --------
#
# Find Gperftools library
#
# ::
#
#   GPerftools_FOUND        - True if the library is found
#   GPerftools_INCLUDE_DIR  - include directory
#   GPerftools_LIBRARY      - GPerftools LIBRARY
#
# ::
#
#   gperftools::profiler
#   gperftools::tcmalloc
#   gperftools::tcmalloc_and_profiler

set(GPerftools_FOUND FALSE)

if(TARGET gperftools::profiler)
  set(GPerftools_FOUND TRUE)
  return()
endif()

find_path(GPerftools_INCLUDE_DIR
  NAMES gperftools/heap-profiler.h
  PATHS /usr /usr/local
)

find_library(GPerftools_profiler_LIBRARY
  NAMES profiler
)

find_library(GPerftools_tcmalloc_LIBRARY
  NAMES tcmalloc
)

find_library(GPerftools_tcmalloc_and_profiler_LIBRARY
  NAMES tcmalloc_and_profiler
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  GPerftools
  REQUIRED_VARS
    GPerftools_INCLUDE_DIR
    GPerftools_profiler_LIBRARY
    GPerftools_tcmalloc_and_profiler_LIBRARY
    GPerftools_tcmalloc_LIBRARY
)
mark_as_advanced(
  GPerftools_INCLUDE_DIR
  GPerftools_profiler_LIBRARY
  GPerftools_tcmalloc_and_profiler_LIBRARY
  GPerftools_tcmalloc_LIBRARY
)

if (GPerftools_FOUND AND NOT TARGET gperftools::profiler)
  add_library(gperftools::profiler UNKNOWN IMPORTED)
  set_target_properties(gperftools::profiler
    PROPERTIES
    IMPORTED_LOCATION ${GPerftools_profiler_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES "${GPerftools_INCLUDE_DIR}"
  )
  add_library(gperftools::tcmalloc UNKNOWN IMPORTED)
  set_target_properties(gperftools::tcmalloc
    PROPERTIES
    IMPORTED_LOCATION ${GPerftools_tcmalloc_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES "${GPerftools_INCLUDE_DIR}"
  )
  add_library(gperftools::tcmalloc_and_profiler UNKNOWN IMPORTED)
  set_target_properties(gperftools::tcmalloc_and_profiler
    PROPERTIES
    IMPORTED_LOCATION ${GPerftools_tcmalloc_and_profiler_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES "${GPerftools_INCLUDE_DIR}"
  )
endif()
