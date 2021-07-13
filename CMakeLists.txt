# Call CMake from build directory (e.g. ./cmake-build-debug) with:
# - for VS-2017: cmake -G "Visual Studio 15 2017" -A x64 -DCMAKE_BUILD_TYPE=Debug ..
# - for VS-2019: cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Debug ..

cmake_minimum_required(VERSION 3.16)

project(erp-processing-context)

# Perform a Debug build by default,
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(NOT ERP_BUILD_VERSION)
    message(WARNING "ERP_BUILD_VERSION not set.")
    set(ERP_BUILD_VERSION "LOCAL VERSION")
else()
    message(NOTICE "ERP_BUILD_VERSION: ${ERP_BUILD_VERSION}")
endif()

if(NOT ERP_RELEASE_VERSION)
    message(WARNING "ERP_RELEASE_VERSION not set.")
    set(ERP_RELEASE_VERSION "UNRELEASED VERSION")
else()
    message(NOTICE "ERP_RELEASE_VERSION: ${ERP_RELEASE_VERSION}")
endif()

string(TIMESTAMP ERP_RELEASE_DATE "%Y-%m-%dT%H:%M:%S+00:00" UTC)
message(NOTICE "ERP_RELEASE_DATE: ${ERP_RELEASE_DATE}")

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
    message(SYSTEM "installing conan packages")
    execute_process(
            COMMAND           conan remote add nexus https://nexus.epa-dev.net/repository/conan-center-proxy true --force
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    )
    execute_process(
            COMMAND           conan remote add erp https://nexus.epa-dev.net/repository/erp-conan-internal true --force
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    )
    execute_process(
            COMMAND           conan install . --build missing --install-folder "${CMAKE_BINARY_DIR}"
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    )
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

include(GNUInstallDirs)

include(cmake/install_libs_for.cmake)
#
# build configuration for all erp build targets
#
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
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
    #Workaround: Some 3rd party libraries (e.g. openssl) are compiled with built pdb debug info but the pdb file is not generated.
    add_link_options(-ignore:4099)
    #Workaround: Couldn't figure out how the following two libraries became part of the list of libs for the linker input.
    add_link_options(-NODEFAULTLIB:libboost_date_time-vc142-mt-gd-x64-1_74.lib)
    add_link_options(-NODEFAULTLIB:libboost_regex-vc142-mt-gd-x64-1_74.lib)
else()
    add_compile_options(-Wall) # This may require an additional -g3 to activate debug symbols for boost::beast
    add_compile_options(-Wextra -Wpedantic)
    add_compile_options(-Werror)
    # explicitly unset 'as-needed' which should be the linker default.
    # Ubuntu overrides the default in the compiler specs, but this makes relocating the libs pulled via conan very hard
    add_link_options(-Wl,--no-as-needed)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-c11-extensions)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_subdirectory(resources)
add_subdirectory(src/erp)
add_subdirectory(src/mock)
add_subdirectory(tools)
add_subdirectory(test)
