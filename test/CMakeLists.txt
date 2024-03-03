cmake_minimum_required(VERSION 3.6)

find_package(GTest REQUIRED)
find_program(MEMCHECK_PROG valgrind)

set(MEMCHECK_CMD ${MEMCHECK_PROG} --leak-check=full --error-exitcode=1)

set(ALL_TESTS
    test_file
    test_sleep
    test_wait
    test_latch
    test_go
    test_generic_awaiter
    test_future
    test_http
    test_semaphore
    test_mutex
    test_shared_mutex
    test_dag
)

include (../cmake/find-workflow.cmake)

foreach(exec_name ${ALL_TESTS})
    add_executable(${exec_name} ${exec_name}.cpp)
    add_test(NAME ${exec_name} COMMAND ${exec_name})

    if (NOT ${MEMCHECK_PROG} STREQUAL "MEMCHECK_PROG-NOTFOUND")
        add_test(NAME mem_${exec_name} COMMAND ${MEMCHECK_CMD} ./${exec_name})
    endif ()

    target_link_directories(${exec_name} PRIVATE ${WORKFLOW_LIB_DIR})

    target_include_directories(${exec_name} PRIVATE
        ${WORKFLOW_INCLUDE_DIR}
        ${OPENSSL_INCLUDE_DIR}
    )

    target_link_libraries(${exec_name}
        ${COKE_LIBRARY}
        ${COKE_TRY_STATIC_WORKFLOW}
        Threads::Threads
        OpenSSL::SSL OpenSSL::Crypto
        GTest::GTest GTest::Main
    )
endforeach()
