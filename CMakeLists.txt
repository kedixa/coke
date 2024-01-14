cmake_minimum_required(VERSION 3.6)

project(
    coke
    VERSION 0.1.0
    LANGUAGES CXX
)

set(COKE_ENABLE_TEST FALSE CACHE BOOL "Whether to build and run tests, default FALSE")
set(COKE_ENABLE_BENCHMARK FALSE CACHE BOOL "Whether to build benchmarks, default FALSE")
set(COKE_ENABLE_EXAMPLE FALSE CACHE BOOL "Whether to build examples, default FALSE")
set(COKE_BUILD_STATIC TRUE CACHE BOOL "Whether to build coke static library, default TRUE")
set(COKE_BUILD_SHARED FALSE CACHE BOOL "Whether to build coke shared library, default FALSE")
set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "cmake build type")

set(COKE_LIBRARY ${PROJECT_NAME})
set(COKE_LIBRARY_DIR ${PROJECT_BINARY_DIR}/lib)
set(COKE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(COKE_INSTALL_CMAKE_FILE ${PROJECT_BINARY_DIR}/${COKE_LIBRARY}-config.install.cmake)
set(COKE_LIBRARY_TARGETS ${COKE_LIBRARY}Targets)

if (COKE_BUILD_STATIC)
    set(COKE_STATIC_LIBRARY ${COKE_LIBRARY}-static)
endif ()

if (COKE_BUILD_SHARED)
    set(COKE_SHARED_LIBRARY ${COKE_LIBRARY}-shared)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${COKE_LIBRARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${COKE_LIBRARY_DIR})

if (NOT WIN32)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
endif ()

if (NOT WORKFLOW_INCLUDE_DIR)
    find_package(Workflow REQUIRED)
endif ()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

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

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(LIBRARY_NAME ${COKE_LIBRARY})
set(INCLUDE_DIR ${COKE_INCLUDE_DIR})
set(LIBRARY_DIR ${COKE_LIBRARY_DIR})
configure_package_config_file(
    cmake/${COKE_LIBRARY}-config.cmake.in
    ${PROJECT_BINARY_DIR}/${COKE_LIBRARY}-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${COKE_LIBRARY}
    PATH_VARS INCLUDE_DIR LIBRARY_DIR LIBRARY_NAME
)

set(INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR})
set(LIBRARY_DIR ${CMAKE_INSTALL_LIBDIR})
configure_package_config_file(
    cmake/${COKE_LIBRARY}-config.install.cmake.in
    ${COKE_INSTALL_CMAKE_FILE}
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${COKE_LIBRARY}
    PATH_VARS INCLUDE_DIR LIBRARY_DIR LIBRARY_NAME
)

if (COKE_BUILD_STATIC)
    install(TARGETS
        ${COKE_STATIC_LIBRARY}
        EXPORT ${COKE_LIBRARY_TARGETS}
        LIBRARY DESTINATION lib
    )
endif ()

if (COKE_BUILD_SHARED)
    install(TARGETS
        ${COKE_SHARED_LIBRARY}
        EXPORT ${COKE_LIBRARY_TARGETS}
        LIBRARY DESTINATION lib
    )
endif ()

install(EXPORT ${COKE_LIBRARY_TARGETS} FILE ${COKE_LIBRARY}-targets.cmake
    NAMESPACE coke:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${COKE_LIBRARY}
)

install(DIRECTORY
    ${COKE_INCLUDE_DIR}/coke
    DESTINATION include
)

install(FILES ${COKE_INSTALL_CMAKE_FILE}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${COKE_LIBRARY}
    RENAME ${COKE_LIBRARY}-config.cmake
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
