cmake_minimum_required(VERSION 3.6)

project(
    coke
    VERSION 0.1.0
    LANGUAGES CXX
)

set(COKE_ENABLE_TEST FALSE CACHE BOOL "Whether to build and run tests, default FALSE")
set(COKE_ENABLE_BENCHMARK FALSE CACHE BOOL "Whether to build benchmarks, default FALSE")
set(COKE_ENABLE_EXAMPLE FALSE CACHE BOOL "Whether to build examples, default FALSE")
set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "cmake build type")

set(COKE_LIBRARY_NAME ${PROJECT_NAME})
set(COKE_LIBRARY_DIR ${PROJECT_BINARY_DIR}/lib)
set(COKE_INCLUDE_DIR ${PROJECT_BINARY_DIR}/include)
set(COKE_INSTALL_CMAKE_FILE ${PROJECT_BINARY_DIR}/${COKE_LIBRARY_NAME}-config.install.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${COKE_LIBRARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${COKE_LIBRARY_DIR})

file(COPY
    ${PROJECT_SOURCE_DIR}/include/coke
    DESTINATION
    ${COKE_INCLUDE_DIR}
)

find_package(Workflow REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
include_directories(${WORKFLOW_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${COKE_INCLUDE_DIR})

# Try to find static workflow
set(BACKUP_CMAKE_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
find_library(COKE_TRY_STATIC_WORKFLOW workflow PATHS ${WORKFLOW_LIB_DIR})
set(CMAKE_FIND_LIBRARY_SUFFIXES ${BACKUP_CMAKE_LIBRARY_SUFFIXES})

if (COKE_TRY_STATIC_WORKFLOW STREQUAL "COKE_TRY_STATIC_WORKFLOW-NOTFOUND")
    message(FATAL_ERROR "Workflow Not Found")
else ()
    message("Found Workflow: " ${COKE_TRY_STATIC_WORKFLOW})
endif()

add_subdirectory(src)

if (COKE_ENABLE_EXAMPLE)
    add_subdirectory(example)
else ()
    add_subdirectory(example EXCLUDE_FROM_ALL)
    message("examples are not included in the default build target.")
endif()

if (COKE_ENABLE_BENCHMARK)
    add_subdirectory(benchmark)
else ()
    add_subdirectory(benchmark EXCLUDE_FROM_ALL)
    message("benchmarks are not included in the default build target.")
endif()

if (COKE_ENABLE_TEST)
    enable_testing()
    add_subdirectory(test)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(LIBRARY_NAME ${COKE_LIBRARY_NAME})
set(INCLUDE_DIR ${COKE_INCLUDE_DIR})
set(LIBRARY_DIR ${COKE_LIBRARY_DIR})
configure_package_config_file(
    ${COKE_LIBRARY_NAME}-config.cmake.in
    ${PROJECT_BINARY_DIR}/${COKE_LIBRARY_NAME}-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${COKE_LIBRARY_NAME}
    PATH_VARS INCLUDE_DIR LIBRARY_DIR LIBRARY_NAME
)

set(INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR})
set(LIBRARY_DIR ${CMAKE_INSTALL_LIBDIR})
configure_package_config_file(
    ${COKE_LIBRARY_NAME}-config.cmake.in
    ${COKE_INSTALL_CMAKE_FILE}
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${COKE_LIBRARY_NAME}
    PATH_VARS INCLUDE_DIR LIBRARY_DIR LIBRARY_NAME
)

install(TARGETS
    ${COKE_LIBRARY_NAME}
    LIBRARY DESTINATION lib
)

install(DIRECTORY
    ${COKE_INCLUDE_DIR}/coke
    DESTINATION include)

install(FILES ${COKE_INSTALL_CMAKE_FILE}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${COKE_LIBRARY_NAME}
    RENAME ${COKE_LIBRARY_NAME}-config.cmake
)

set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGE_NAME "coke")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "coke: Coroutine Workflow based on C++ 20")
set(CPACK_PACKAGE_VENDOR "kedixa")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
# set(CPACK_RPM_PACKAGE_GROUP "")
set(CPACK_RPM_PACKAGE_URL "https://github.com/kedixa/coke/")
# set(CPACK_RPM_PACKAGE_DESCRIPTION "")
set(CPACK_RPM_PACKAGE_LICENSE "TODO")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_RPM_PACKAGE_RELEASE "0")

include(CPack)
