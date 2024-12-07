cmake_minimum_required(VERSION 3.16)

set(SRCS
    cancelable_timer.cpp
    coke_impl.cpp
    condition.cpp
    dag.cpp
    fileio.cpp
    go.cpp
    http_impl.cpp
    latch.cpp
    mutex.cpp
    mysql_impl.cpp
    qps_pool.cpp
    random.cpp
    redis_impl.cpp
    sleep.cpp
    stop_token.cpp
    sync_guard.cpp
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
    set_target_properties(${COKE_STATIC_LIBRARY} PROPERTIES POSITION_INDEPENDENT_CODE ON)

    add_library(coke::${COKE_STATIC_LIBRARY} ALIAS ${COKE_STATIC_LIBRARY})
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

    add_library(coke::${COKE_SHARED_LIBRARY} ALIAS ${COKE_SHARED_LIBRARY})
endif ()

if (COKE_BUILD_STATIC)
    add_library(coke::${COKE_LIBRARY} ALIAS ${COKE_STATIC_LIBRARY})
elseif (COKE_BUILD_SHARED)
    add_library(coke::${COKE_LIBRARY} ALIAS ${COKE_SHARED_LIBRARY})
endif ()
