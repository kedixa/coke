cmake_minimum_required(VERSION 3.16)

add_library(coke-objs OBJECT
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

# In subdirectory mode, build after workflow
if (TARGET workflow-static)
    add_dependencies(coke-objs workflow-static)
endif ()

if (TARGET workflow-shared)
    add_dependencies(coke-objs workflow-shared)
endif ()

add_library(coke-wfhdrs INTERFACE)

if (TARGET coke::workflow-static)
    target_include_directories(coke-wfhdrs
        SYSTEM
        INTERFACE
            $<TARGET_PROPERTY:coke::workflow-static,INTERFACE_INCLUDE_DIRECTORIES>
    )
elseif (TARGET coke::workflow-shared)
    target_include_directories(coke-wfhdrs
        SYSTEM
        INTERFACE
            $<TARGET_PROPERTY:coke::workflow-static,INTERFACE_INCLUDE_DIRECTORIES>
    )
endif ()

target_link_libraries(coke-objs PRIVATE coke-wfhdrs)

target_include_directories(coke-objs
    PRIVATE
        $<BUILD_INTERFACE:${COKE_INCLUDE_DIR}>
)

set_target_properties(coke-objs PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (COKE_BUILD_STATIC)
    if (NOT TARGET coke::workflow-static)
        message(FATAL_ERROR "Requires workflow-static but not found.")
    endif ()

    add_library(${COKE_STATIC_LIBRARY} STATIC $<TARGET_OBJECTS:coke-objs>)

    target_include_directories(${COKE_STATIC_LIBRARY}
        PUBLIC
            $<BUILD_INTERFACE:${COKE_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_link_libraries(${COKE_STATIC_LIBRARY}
        PUBLIC
            coke::workflow-static
    )

    set_target_properties(${COKE_STATIC_LIBRARY}
        PROPERTIES
            OUTPUT_NAME ${COKE_LIBRARY}
    )

    add_library(coke::${COKE_STATIC_LIBRARY} ALIAS ${COKE_STATIC_LIBRARY})
endif ()

if (COKE_BUILD_SHARED)
    if (NOT TARGET coke::workflow-shared)
        message(FATAL_ERROR "Requires workflow-shared but not found.")
    endif ()

    add_library(${COKE_SHARED_LIBRARY} SHARED $<TARGET_OBJECTS:coke-objs>)

    target_include_directories(${COKE_SHARED_LIBRARY}
        PUBLIC
            $<BUILD_INTERFACE:${COKE_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_link_libraries(${COKE_SHARED_LIBRARY}
        PUBLIC
            coke::workflow-shared
    )

    set_target_properties(${COKE_SHARED_LIBRARY}
        PROPERTIES
            OUTPUT_NAME ${COKE_LIBRARY}
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    )

    add_library(coke::${COKE_SHARED_LIBRARY} ALIAS ${COKE_SHARED_LIBRARY})
endif ()

if (COKE_BUILD_STATIC)
    add_library(coke::${COKE_LIBRARY} ALIAS ${COKE_STATIC_LIBRARY})
elseif (COKE_BUILD_SHARED)
    add_library(coke::${COKE_LIBRARY} ALIAS ${COKE_SHARED_LIBRARY})
endif ()
