#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

# Call CMake from build directory (e.g. ./cmake-build-debug) with:
# - for VS-2017: cmake -G "Visual Studio 15 2017" -A x64 -DCMAKE_BUILD_TYPE=Debug ..
# - for VS-2019: cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Debug ..

cmake_minimum_required(VERSION 3.18)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(erp-processing-context LANGUAGES CXX)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src") # "src/Header.hxx"

include(GNUInstallDirs)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

# Perform a Debug build by default,
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 20)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(ERP_WITH_HSM_TPM_PRODUCTION ON CACHE BOOL "Link against the production HSM and TPM from hsmclient and tpmclient libraries.")
set(ERP_WITH_HSM_MOCK OFF CACHE BOOL "Link against mock implementations of HSM and TPM.")

message(NOTICE "ERP_WITH_HSM_TPM_PRODUCTION: ${ERP_WITH_HSM_TPM_PRODUCTION}")
message(NOTICE "ERP_WITH_HSM_MOCK: ${ERP_WITH_HSM_MOCK}")

if (NOT ERP_WITH_HSM_TPM_PRODUCTION AND NOT ERP_WITH_HSM_MOCK)
    message(FATAL_ERROR "either ERP_WITH_HSM_TPM_PRODUCTION or ERP_WITH_HSM_MOCK must be enabled.")
endif()

# GEMREQ-start A_19716#ENABLE_DEBUG_LOG
if((CMAKE_BUILD_TYPE MATCHES Debug) OR (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo))
    add_compile_options(-DENABLE_DEBUG_LOG)
endif()
# GEMREQ-end A_19716#ENABLE_DEBUG_LOG

SET(ERP_BUILD_VERSION "" CACHE STRING "Erp Build version")
SET(ERP_RELEASE_VERSION "" CACHE STRING "Erp Release version")

if(NOT ERP_BUILD_VERSION)
    message(WARNING "ERP_BUILD_VERSION not set.")
    set(ERP_BUILD_VERSION "LOCAL VERSION" CACHE STRING "Erp Build version" FORCE)
else()
    message(NOTICE "ERP_BUILD_VERSION: ${ERP_BUILD_VERSION}")
endif()

if(NOT ERP_RELEASE_VERSION)
    message(FATAL_ERROR "ERP_RELEASE_VERSION not set")
endif()

### Pattern/length according to A_22470-05
set(ERP_RELEASE_VERSION_PATTERN "^[a-zA-Z0-9.-]+$")
if (NOT ERP_RELEASE_VERSION MATCHES "${ERP_RELEASE_VERSION_PATTERN}")
    message(FATAL_ERROR "ERP_RELEASE_VERSION (\"${ERP_RELEASE_VERSION}\") doesn't match pattern ${ERP_RELEASE_VERSION_PATTERN}")
endif()
string(LENGTH "${ERP_RELEASE_VERSION}" ERP_RELEASE_VERSION_LENGTH)
if (ERP_RELEASE_VERSION_LENGTH GREATER 15)
    message(FATAL_ERROR "ERP_RELEASE_VERSION (\"${ERP_RELEASE_VERSION}\") too long.")
endif()

string(TIMESTAMP ERP_RELEASE_DATE "%Y-%m-%dT%H:%M:%S+00:00" UTC)
message(NOTICE "ERP_RELEASE_DATE: ${ERP_RELEASE_DATE}")

find_package(Boost REQUIRED)
find_package(GTest REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(ZLIB REQUIRED)
find_package(antlr4-runtime REQUIRED)
find_package(botan REQUIRED)
find_package(date REQUIRED)
find_package(glog REQUIRED)
find_package(gsl-lite REQUIRED)
find_package(hiredis REQUIRED)
find_package(libpqxx REQUIRED)
find_package(libunwind REQUIRED)
find_package(libxml2 REQUIRED)
find_package(magic_enum REQUIRED)
find_package(redis++ REQUIRED)
find_package(zstd REQUIRED)

option(WITH_PROFILING "Build binaries with profiling" OFF)
if (WITH_PROFILING)
    find_package(GPerftools REQUIRED)
endif()

if (NOT DEFINED ANTLR_EXECUTABLE)
    set(ANTLR_EXECUTABLE "${CMAKE_BINARY_DIR}/antlr-4.10.1-complete.jar" CACHE STRING
            "Path of ANTLR4 executable. Can be either normal executable or jar file.")
    set(ANTLR_URL "https://www.antlr.org/download/antlr-4.10.1-complete.jar" CACHE STRING
            "Download URL for ALNTL4 genertor executable.")
    set(ANTLR_EXECUTABLE_SHA256 "41949d41f20d31d5b8277187735dd755108df52b38db6c865108d3382040f918" CACHE STRING
            "SHA256 hash of downloaded ANTLR4 executable")
    if (NOT EXISTS "${ANTLR_EXECUTABLE}")
        FILE(DOWNLOAD
                "${ANTLR_URL}" "${ANTLR_EXECUTABLE}"
                EXPECTED_HASH SHA256=${ANTLR_EXECUTABLE_SHA256}
            )
    endif()
endif()

if (ERP_WITH_HSM_TPM_PRODUCTION)
    find_package(tpmclient)
    if(NOT tpmclient_FOUND)
        message(FATAL_ERROR "Package 'tpmclient' not found, building against production HSM can be disabled with ERP_WITH_HSM_TPM_PRODUCTION=OFF")
    endif()
    find_package(hsmclient)
    if (NOT hsmclient_FOUND)
        message(FATAL_ERROR "Package `hsmclient` not found, building against production HSM can be disabled with ERP_WITH_HSM_TPM_PRODUCTION=OFF")
    endif()
endif()


include(cmake/install_libs_for.cmake)

include(cmake/generate_pki_test_data.cmake)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/antlr")
find_package(ANTLR REQUIRED)


#
# build configuration for all erp build targets
#
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(ERP_WARNING_AS_ERROR OFF CACHE BOOL "Treat all warnings as errors")
message(NOTICE "ERP_WARNING_AS_ERROR: ${ERP_WARNING_AS_ERROR}")

# allow logging in ePA code
add_compile_definitions(ENABLE_LOG=1)
# enable std::string support in RapidJSON across all targets
add_compile_definitions(RAPIDJSON_HAS_STDSTRING=1)
add_compile_definitions(RAPIDJSON_SCHEMA_VERBOSE=0)
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    #-Wall produces to much warnings when include the 3rd party library header files
    add_compile_options(-W4)
    # Suppress warning C4200 : "Non-standard extension: Null array in structure/union" when including "hiredis/sds.h"
    add_compile_options(-wd4200)
    # Suppress warning C4200 : "Conditional expression is constant: when compiling the class template member function "void rapidjson::internal::TokenHelper"
    add_compile_options(-wd4127)
    # Enable __cplusplus preprocessor macro to use std::string_view and std::optional in redis++.
    add_compile_options(-Zc:__cplusplus)
    add_compile_options(-FIciso646)
    add_compile_options(-D_WIN32_WINNT=0x0601)
    add_compile_options(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
    add_compile_options(-D_CRT_SECURE_NO_WARNINGS)
    if(ERP_WARNING_AS_ERROR)
        add_compile_options(/WX)
    endif()
    #Workaround: Some 3rd party libraries (e.g. openssl) are compiled with built pdb debug info but the pdb file is not generated.
    add_link_options(-ignore:4099)
    #Workaround: Couldn't figure out how the following two libraries became part of the list of libs for the linker input.
    add_link_options(-NODEFAULTLIB:libboost_date_time-vc142-mt-gd-x64-1_74.lib)
    add_link_options(-NODEFAULTLIB:libboost_regex-vc142-mt-gd-x64-1_74.lib)
else()
    add_compile_options(-Wall) # This may require an additional -g3 to activate debug symbols for boost::beast
    add_compile_options(-Wextra -Wpedantic -Wconversion -Wno-deprecated-declarations)
    # disable -Wrestict for gcc, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105651
    if(CMAKE_BUILD_TYPE MATCHES Release)
        add_compile_options($<$<CXX_COMPILER_ID:GNU>:-Wno-restrict> $<$<CXX_COMPILER_ID:GNU>:-Wno-stringop-overread>)
    endif()
    if(ERP_WARNING_AS_ERROR)
        add_compile_options(-Werror)
    endif()
    # explicitly unset 'as-needed' which should be the linker default.
    # Ubuntu overrides the default in the compiler specs, but this makes relocating the libs pulled via conan very hard
    if (NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
        add_link_options(-Wl,--no-as-needed)
    endif()
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-c11-extensions)
endif()

add_subdirectory(resources)
add_subdirectory(scripts)
add_subdirectory(src/shared)
add_subdirectory(src/tee3)
add_subdirectory(src/fhirtools)
add_subdirectory(src/exporter)
add_subdirectory(src/erp)
add_subdirectory(src/mock)
add_subdirectory(tools)
add_subdirectory(test)
