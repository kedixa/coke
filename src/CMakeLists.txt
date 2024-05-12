cmake_minimum_required(VERSION 3.6)

set(SRCS
    coke_impl.cpp
    fileio.cpp
    go.cpp
    http_impl.cpp
    latch.cpp
    qps_pool.cpp
    redis_impl.cpp
    sleep.cpp
    mysql_impl.cpp
    cancelable_timer.cpp
    mutex.cpp
    dag.cpp
    condition.cpp
)

if (COKE_BUILD_STATIC)
    add_library(${COKE_STATIC_LIBRARY} STATIC ${SRCS})

    target_include_directories(${COKE_STATIC_LIBRARY} PUBLIC
        $<BUILD_INTERFACE:${WORKFLOW_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${OPENSSL_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${COKE_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )

    set_target_properties(${COKE_STATIC_LIBRARY} PROPERTIES OUTPUT_NAME ${COKE_LIBRARY})
endif ()

if (COKE_BUILD_SHARED)
    add_library(${COKE_SHARED_LIBRARY} SHARED ${SRCS})

    target_include_directories(${COKE_SHARED_LIBRARY} PUBLIC
        $<BUILD_INTERFACE:${WORKFLOW_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${OPENSSL_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${COKE_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )

    set_target_properties(${COKE_SHARED_LIBRARY}
        PROPERTIES OUTPUT_NAME ${COKE_LIBRARY}
        VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif ()

if (COKE_BUILD_STATIC)
    add_library(${COKE_LIBRARY} ALIAS ${COKE_STATIC_LIBRARY})
elseif (COKE_BUILD_SHARED)
    add_library(${COKE_LIBRARY} ALIAS ${COKE_SHARED_LIBRARY})
endif ()

