#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

# Call CMake from build directory (e.g. ./cmake-build-debug) with:
# - for VS-2017: cmake -G "Visual Studio 15 2017" -A x64 -DCMAKE_BUILD_TYPE=Debug ..
# - for VS-2019: cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Debug ..

cmake_minimum_required(VERSION 3.18)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(erp-processing-context LANGUAGES CXX)

# Perform a Debug build by default,
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 20)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(ERP_CONAN_PROFILE "" CACHE STRING "The conan profile to use. Leave empty for default profile.")

set(ERP_WITH_HSM_TPM_PRODUCTION ON CACHE BOOL "Link against the production HSM and TPM from hsmclient and tpmclient libraries.")
set(ERP_WITH_HSM_MOCK OFF CACHE BOOL "Link against mock implementations of HSM and TPM.")

message(NOTICE "ERP_WITH_HSM_TPM_PRODUCTION: ${ERP_WITH_HSM_TPM_PRODUCTION}")
message(NOTICE "ERP_WITH_HSM_MOCK: ${ERP_WITH_HSM_MOCK}")

if (NOT ERP_WITH_HSM_TPM_PRODUCTION AND NOT ERP_WITH_HSM_MOCK)
    message(FATAL_ERROR "either ERP_WITH_HSM_TPM_PRODUCTION or ERP_WITH_HSM_MOCK must be enabled.")
endif()

if((CMAKE_BUILD_TYPE MATCHES Debug) OR (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo))
    add_compile_options(-DENABLE_DEBUG_LOG)
endif()

SET(ERP_BUILD_VERSION "" CACHE STRING "Erp Build version")
SET(ERP_RELEASE_VERSION "" CACHE STRING "Erp Release version")

if(NOT ERP_BUILD_VERSION)
    message(WARNING "ERP_BUILD_VERSION not set.")
    set(ERP_BUILD_VERSION "LOCAL VERSION" CACHE STRING "Erp Build version" FORCE)
else()
    message(NOTICE "ERP_BUILD_VERSION: ${ERP_BUILD_VERSION}")
endif()

if(NOT ERP_RELEASE_VERSION)
    message(WARNING "ERP_RELEASE_VERSION not set.")
    set(ERP_RELEASE_VERSION "UNRELEASED VERSION" CACHE STRING "Erp Release version" FORCE)
else()
    message(NOTICE "ERP_RELEASE_VERSION: ${ERP_RELEASE_VERSION}")
endif()

string(TIMESTAMP ERP_RELEASE_DATE "%Y-%m-%dT%H:%M:%S+00:00" UTC)
message(NOTICE "ERP_RELEASE_DATE: ${ERP_RELEASE_DATE}")
option(WITH_PROFILING "Build binaries with profiling" OFF)
if (WITH_PROFILING)
    find_package(GPerftools REQUIRED)
endif()

#
# Conan: including 3rd party packages by running "conan install" on the fly
#
# The following conan profile settings have been used to build on ubuntu 20.04:
# [settings]
# os=Linux
# os_build=Linux
# arch=x86_64
# arch_build=x86_64
# compiler=gcc
# compiler.version=9
# compiler.libcxx=libstdc++11
# compiler.cppstd=17
# build_type=Release
# [options]
# [build_requires]
# [env]


if(NOT EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    find_program(CONAN NAMES conan DOC "The path to the conan tool.")
    if(NOT CONAN)
        message(FATAL_ERROR "conan is required to manage dependencies.")
    endif()
    message(SYSTEM "installing conan packages")
    if(NOT ERP_WITH_HSM_TPM_PRODUCTION)
        set(CONAN_HSM_TPM -o with_tpmclient=False -o with_hsmclient=False)
    else()
        set(CONAN_HSM_TPM -o with_tpmclient=True -o with_hsmclient=True)
    endif()

    if(ERP_CONAN_PROFILE AND NOT "${ERP_CONAN_PROFILE}" STREQUAL "")
        set(CONAN_PROFILE --profile "${ERP_CONAN_PROFILE}")
    endif()

    message(NOTICE "Installing conan packages")
    execute_process(
            COMMAND           "${CONAN}" remote add nexus https://nexus.epa-dev.net/repository/conan-center-proxy true --force
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    )
    execute_process(
            COMMAND           "${CONAN}" remote add erp https://nexus.epa-dev.net/repository/erp-conan-internal true --force
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    )
    if (DEFINED CMAKE_CXX_COMPILER_VERSION AND NOT DEFINED CONAN_COMPILER_VERSION)
        string(REGEX REPLACE "\\..*" "" CONAN_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    if (NOT DEFINED CONAN_COMPILER_VERSION)
        message(FATAL_ERROR "cannot determine compiler version. Please set CONAN_COMPILER_VERSION accordingly.")
    endif()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(COMPILER_LIBCXX_SETTING "libc++")
    else()
        set(COMPILER_LIBCXX_SETTING "libstdc++11")
    endif()

    separate_arguments(ERP_CONAN_ARG_LIST NATIVE_COMMAND ${ERP_CONAN_ARGS})
    execute_process(
        COMMAND env CONAN_CMAKE_GENERATOR=${CMAKE_GENERATOR}
                "${CONAN}" install
                --build outdated
                --install-folder "${CMAKE_BINARY_DIR}"
                -s compiler.cppstd=${CMAKE_CXX_STANDARD}
                -s compiler.libcxx=${COMPILER_LIBCXX_SETTING}
                -s compiler.version=${CONAN_COMPILER_VERSION}
                -e CXX=${CMAKE_CXX_COMPILER}
                -e CC=${CMAKE_C_COMPILER}
                ${CONAN_HSM_TPM}
                ${CONAN_PROFILE}
                ${ERP_CONAN_ARG_LIST}
                "${CMAKE_SOURCE_DIR}"
    )
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

# Note for windows users:
# -----------------------
#
# Since the hsm client is included the generated "conanbuildinfo.cmake" may no longer contain the
# correct path to the boost header files. As a workaround the "conanbuildinfo.cmake" file may need
# to be manually patched as follows:
#
# Wrongly created are the following entries:
#
#     set(CONAN_BOOST_ROOT "C:/Users/<User>/.conan/data/boost/1.74.0/_/_/package/524ea35a8120baabdde02483add58d81bf541327")
#     set(CONAN_INCLUDE_DIRS_BOOST )
#     set(CONAN_BUILD_DIRS_BOOST "C:/Users/<User>/.conan/data/boost/1.74.0/_/_/package/524ea35a8120baabdde02483add58d81bf541327/")
#
# Correct them with:
#
#     set(CONAN_BOOST_ROOT "<conan_link>")
#     set(CONAN_INCLUDE_DIRS_BOOST "<conan_link>/include")
#     set(CONAN_BUILD_DIRS_BOOST "<conan_link>")
#
#     As <conan_link> use the content of file "C:/Users/<User>/.conan/data/boost/1.74.0/_/_/package/524ea35a8120baabdde02483add58d81bf541327/.conan_link"
#     E.g. (replacing "\" with "/"):
#       set(CONAN_BOOST_ROOT "C:/.conan/741c2f/1")
#       set(CONAN_INCLUDE_DIRS_BOOST "C:/.conan/741c2f/1/include")
#       set(CONAN_BUILD_DIRS_BOOST "C:/.conan/741c2f/1")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

if (ERP_WITH_HSM_TPM_PRODUCTION)
    if("${CONAN_HSMCLIENT_ROOT}" STREQUAL "")
        message(FATAL_ERROR "CONAN_HSMCLIENT_ROOT not found, building against production HSM can be disabled with ERP_WITH_HSM_TPM_PRODUCTION=OFF")
    endif()
    if ("${CONAN_TPMCLIENT_ROOT}" STREQUAL "")
        message(FATAL_ERROR "CONAN_TPMCLIENT_ROOT not found, building against production HSM can be disabled with ERP_WITH_HSM_TPM_PRODUCTION=OFF")
    endif()
endif()

include(GNUInstallDirs)

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
add_subdirectory(src/fhirtools)
add_subdirectory(src/erp)
add_subdirectory(src/mock)
add_subdirectory(tools)
add_subdirectory(test)
