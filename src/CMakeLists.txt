cmake_minimum_required(VERSION 3.16)

set(SRCS
    cancelable_timer.cpp
    coke_impl.cpp
    condition.cpp
    dag.cpp
    fileio.cpp
    go.cpp
    hashtable.cpp
    http_impl.cpp
    latch.cpp
    mutex.cpp
    mysql_impl.cpp
    qps_pool.cpp
    random.cpp
    rbtree.cpp
    sleep.cpp
    stop_token.cpp
    str_packer.cpp
    sync_guard.cpp

    net/client_conn_info.cpp
    net/tlv_client.cpp
    net/tlv_task.cpp

    nspolicy/nspolicy.cpp
    nspolicy/weighted_random_policy.cpp
    nspolicy/weighted_least_conn_policy.cpp
    nspolicy/weighted_round_robin_policy.cpp

    redis/client_task.cpp
    redis/client.cpp
    redis/cluster_client.cpp
    redis/message.cpp
    redis/parser.cpp
    redis/value.cpp
)

if (COKE_BUILD_STATIC)
    add_library(${COKE_STATIC_LIBRARY} STATIC ${SRCS})

    target_include_directories(${COKE_STATIC_LIBRARY}
        PUBLIC
            $<BUILD_INTERFACE:${WORKFLOW_INCLUDE_DIR}>
            $<BUILD_INTERFACE:${COKE_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_link_directories(${COKE_STATIC_LIBRARY}
        PUBLIC
            $<BUILD_INTERFACE:${WORKFLOW_LIB_DIR}>
    )

    target_link_libraries(${COKE_STATIC_LIBRARY}
        PUBLIC
            OpenSSL::SSL OpenSSL::Crypto
            Threads::Threads
    )

    set_target_properties(${COKE_STATIC_LIBRARY}
        PROPERTIES
            OUTPUT_NAME ${COKE_LIBRARY}
            POSITION_INDEPENDENT_CODE ON
    )

    add_library(coke::${COKE_STATIC_LIBRARY} ALIAS ${COKE_STATIC_LIBRARY})
endif ()

if (COKE_BUILD_SHARED)
    add_library(${COKE_SHARED_LIBRARY} SHARED ${SRCS})

    target_include_directories(${COKE_SHARED_LIBRARY}
        PUBLIC
            $<BUILD_INTERFACE:${WORKFLOW_INCLUDE_DIR}>
            $<BUILD_INTERFACE:${COKE_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_link_directories(${COKE_SHARED_LIBRARY}
        PUBLIC
            $<BUILD_INTERFACE:${WORKFLOW_LIB_DIR}>
    )

    target_link_libraries(${COKE_SHARED_LIBRARY}
        PUBLIC
            workflow
            OpenSSL::SSL OpenSSL::Crypto
            Threads::Threads
    )

    set_target_properties(${COKE_SHARED_LIBRARY}
        PROPERTIES
            OUTPUT_NAME ${COKE_LIBRARY}
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
    )

    add_library(coke::${COKE_SHARED_LIBRARY} ALIAS ${COKE_SHARED_LIBRARY})
endif ()

if (COKE_BUILD_STATIC)
    add_library(coke::${COKE_LIBRARY} ALIAS ${COKE_STATIC_LIBRARY})
elseif (COKE_BUILD_SHARED)
    add_library(coke::${COKE_LIBRARY} ALIAS ${COKE_SHARED_LIBRARY})
endif ()
