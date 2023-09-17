cmake_minimum_required(VERSION 3.6)

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
)

foreach(example ${ALL_EXAMPLES})
    string(REGEX MATCH "[^-]*$" exec_name ${example})
    add_executable(${exec_name} ${example}.cpp)

    target_link_directories(${exec_name} PRIVATE ${WORKFLOW_LIB_DIR})
    target_link_options(${exec_name} PRIVATE "-Wl,-rpath,${WORKFLOW_LIB_DIR}")

    target_link_libraries(${exec_name}
        ${COKE_LIBRARY_NAME}
        workflow
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
