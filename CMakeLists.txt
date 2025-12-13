###########################################################
# Coke Library CMake Configuration
#
# This file set the following variables:
# - COKE_VERSION: Coke library version
# - COKE_MAJOR_VERSION: Coke library major version
# - COKE_MINOR_VERSION: Coke library minor version
# - COKE_PATCH_VERSION: Coke library patch version
# - COKE_INCLUDE_DIR: Coke include directory
# - COKE_LIBRARY_DIR: Coke library output directory
#
# This file exports the following targets:
# - coke::coke-static If COKE_BUILD_STATIC is ON
# - coke::coke-shared If COKE_BUILD_SHARED is ON
# - coke::coke Alias of coke::coke-static or coke::coke-shared
#
# Configurable build options:
# - COKE_BUILD_STATIC: Build static library (default: ON)
# - COKE_BUILD_SHARED: Build shared library (default: OFF)
# - COKE_ENABLE_TEST: Build tests (default: OFF)
# - COKE_ENABLE_BENCHMARK: Build benchmarks (default: OFF)
# - COKE_ENABLE_EXAMPLE: Build examples (default: OFF)
#
# Override options using -D, e.g., -DCOKE_ENABLE_EXAMPLE=ON
###########################################################

cmake_minimum_required(VERSION 3.16)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "CMake build type")
endif()

if (NOT DEFINED PROJECT_NAME)
    set(COKE_MASTER_PROJECT ON)
else ()
    set(COKE_MASTER_PROJECT OFF)
endif ()

project(
    coke
    VERSION 0.6.0
    LANGUAGES CXX
)

include(GNUInstallDirs)

if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif ()

# Build Options

option(COKE_BUILD_STATIC "Build coke static library." ON)
option(COKE_BUILD_SHARED "Build coke shared library." OFF)
option(COKE_ENABLE_TEST "Build coke tests." OFF)
option(COKE_ENABLE_BENCHMARK "Build coke benchmarks." OFF)
option(COKE_ENABLE_EXAMPLE "Build coke examples" OFF)

set(COKE_WORKFLOW_SRC_DIR "" CACHE STRING "Find workflow source code in subdir.")

# Variables

set(COKE_LIBRARY coke)
set(COKE_STATIC_LIBRARY ${COKE_LIBRARY}-static)
set(COKE_SHARED_LIBRARY ${COKE_LIBRARY}-shared)

set(COKE_VERSION ${coke_VERSION} CACHE STRING "Coke library version")
set(COKE_MAJOR_VERSION ${coke_VERSION_MAJOR} CACHE STRING "Coke major version")
set(COKE_MINOR_VERSION ${coke_VERSION_MINOR} CACHE STRING "Coke minor version")
set(COKE_PATCH_VERSION ${coke_VERSION_PATCH} CACHE STRING "Coke patch version")

set(COKE_LIBRARY_DIR ${PROJECT_BINARY_DIR}/lib CACHE STRING "Coke library dir")
set(COKE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include CACHE STRING "Coke include dir")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${COKE_LIBRARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${COKE_LIBRARY_DIR})

set(COKE_CONFIG_INPUT_FILE ${PROJECT_SOURCE_DIR}/cmake/coke-config.cmake.in)
set(COKE_FIND_WORKFLOW_INPUT_FILE ${PROJECT_SOURCE_DIR}/cmake/find-workflow.cmake)

set(COKE_VERSION_OUTPUT_FILE ${PROJECT_BINARY_DIR}/coke-config-version.cmake)
set(COKE_CONFIG_OUTPUT_FILE ${PROJECT_BINARY_DIR}/coke-config.cmake)
set(COKE_FIND_WORKFLOW_OUTPUT_FILE ${PROJECT_BINARY_DIR}/find-workflow.cmake)
set(COKE_INSTALL_CONFIG_OUTPUT_FILE ${PROJECT_BINARY_DIR}/coke-config.install.cmake)
set(COKE_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/coke)

# Build

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-interference-size")
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
endif ()

if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
endif ()

if (COKE_WORKFLOW_SRC_DIR)
    add_subdirectory(${COKE_WORKFLOW_SRC_DIR})
endif ()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

include(cmake/find-workflow.cmake)

add_subdirectory(src)

if (COKE_ENABLE_EXAMPLE)
    add_subdirectory(example)
endif ()

if (COKE_ENABLE_BENCHMARK)
    add_subdirectory(benchmark)
endif()

if (COKE_ENABLE_TEST)
    enable_testing()
    add_subdirectory(test)
endif()

# Install and Export

if (COKE_MASTER_PROJECT)
    include(CMakePackageConfigHelpers)

    set(COKE_INSTALL_TARGETS)
    set(COKE_TARGETS_EXPORT_NAME ${COKE_LIBRARY}-targets)

    if (TARGET ${COKE_STATIC_LIBRARY})
        list(APPEND COKE_INSTALL_TARGETS ${COKE_STATIC_LIBRARY})
    endif ()

    if (TARGET ${COKE_SHARED_LIBRARY})
        list(APPEND COKE_INSTALL_TARGETS ${COKE_SHARED_LIBRARY})
    endif ()


    # Generate config files for build tree.
    set(INCLUDE_DIR ${COKE_INCLUDE_DIR})
    set(LIBRARY_DIR ${COKE_LIBRARY_DIR})
    configure_package_config_file(
        ${COKE_CONFIG_INPUT_FILE}
        ${COKE_CONFIG_OUTPUT_FILE}
        INSTALL_DESTINATION ${COKE_CMAKE_INSTALL_DIR}
        PATH_VARS INCLUDE_DIR LIBRARY_DIR
    )

    write_basic_package_version_file(
        ${COKE_VERSION_OUTPUT_FILE}
        VERSION ${COKE_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    # Generate config files for install.
    set(INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR})
    set(LIBRARY_DIR ${CMAKE_INSTALL_LIBDIR})
    configure_package_config_file(
        ${COKE_CONFIG_INPUT_FILE}
        ${COKE_INSTALL_CONFIG_OUTPUT_FILE}
        INSTALL_DESTINATION ${COKE_CMAKE_INSTALL_DIR}
        PATH_VARS INCLUDE_DIR LIBRARY_DIR
    )

    configure_file(
        ${COKE_FIND_WORKFLOW_INPUT_FILE}
        ${COKE_FIND_WORKFLOW_OUTPUT_FILE}
        COPYONLY
    )

    export(
        TARGETS ${COKE_INSTALL_TARGETS}
        NAMESPACE coke::
        FILE ${PROJECT_BINARY_DIR}/${COKE_TARGETS_EXPORT_NAME}.cmake
    )

    install(
        TARGETS ${COKE_INSTALL_TARGETS}
        EXPORT ${COKE_TARGETS_EXPORT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(
        EXPORT ${COKE_TARGETS_EXPORT_NAME}
        FILE ${COKE_TARGETS_EXPORT_NAME}.cmake
        NAMESPACE coke::
        DESTINATION ${COKE_CMAKE_INSTALL_DIR}
    )

    install(
        DIRECTORY ${COKE_INCLUDE_DIR}/coke
        DESTINATION include
    )

    install(
        FILES ${COKE_INSTALL_CONFIG_OUTPUT_FILE}
        DESTINATION ${COKE_CMAKE_INSTALL_DIR}
        RENAME coke-config.cmake
    )

    install(
        FILES
            ${COKE_VERSION_OUTPUT_FILE}
            ${COKE_FIND_WORKFLOW_OUTPUT_FILE}
        DESTINATION ${COKE_CMAKE_INSTALL_DIR}
    )

    # Package

    set(CPACK_GENERATOR "RPM")
    set(CPACK_PACKAGE_NAME "coke")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "coke: Coroutine Workflow based on C++ 20")
    set(CPACK_PACKAGE_VENDOR "kedixa")
    set(CPACK_PACKAGE_VERSION ${COKE_VERSION})
    set(CPACK_RPM_PACKAGE_URL "https://github.com/kedixa/coke/")
    set(CPACK_RPM_PACKAGE_LICENSE "Apache 2.0")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
    set(CPACK_RPM_PACKAGE_RELEASE "0")

    include(CPack)
endif ()
