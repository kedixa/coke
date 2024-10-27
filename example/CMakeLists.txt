cmake_minimum_required(VERSION 3.16)

set(ALL_EXAMPLES
    ex001-helloworld
    ex002-http_get
    ex003-http_hello_server
    ex004-word_count
    ex005-merge_sort
    ex006-http_proxy
    ex007-redis_cli
    ex008-parallel_http_get
    ex009-mysql_cli
    ex010-timed_wait
    ex011-redis_server
    ex012-faster_merge_sort
    ex013-background_clock
    ex014-option_parser
    ex015-async_queue
    ex016-wait_group
    ex017-backup_request
    ex018-local_cache
)

include (../cmake/find-workflow.cmake)

foreach(example ${ALL_EXAMPLES})
    string(REGEX MATCH "[^-]*$" exec_name ${example})
    add_executable(${exec_name} ${example}.cpp)

    target_link_directories(${exec_name} PRIVATE ${WORKFLOW_LIB_DIR})

    target_include_directories(${exec_name} PRIVATE
        ${WORKFLOW_INCLUDE_DIR}
        ${OPENSSL_INCLUDE_DIR}
    )

    target_link_libraries(${exec_name}
        coke::coke
        ${COKE_TRY_STATIC_WORKFLOW}
        Threads::Threads
        OpenSSL::SSL OpenSSL::Crypto
    )
endforeach()

if (USE_GNU_READLINE)
    set(USE_READLINE_EXAMPLES
        mysql_cli
        redis_cli
    )

    foreach(example ${USE_READLINE_EXAMPLES})
        target_compile_definitions(${example} PRIVATE USE_GNU_READLINE)
        target_link_libraries(${example} readline)
    endforeach()
endif()
