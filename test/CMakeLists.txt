cmake_minimum_required(VERSION 3.6)

find_package(GTest REQUIRED)
find_program(MEMCHECK_PROG valgrind)

set(MEMCHECK_CMD ${MEMCHECK_PROG} --leak-check=full --error-exitcode=1)

set(ALL_TESTS
    test_file
    test_sleep
    test_wait
)

foreach(exec_name ${ALL_TESTS})
    add_executable(${exec_name} ${exec_name}.cpp)
    add_test(NAME ${exec_name} COMMAND ${exec_name})

    if (NOT ${MEMCHECK_PROG} STREQUAL "MEMCHECK_PROG-NOTFOUND")
        add_test(NAME mem_${exec_name} COMMAND ${MEMCHECK_CMD} ./${exec_name})
    endif ()

    target_link_directories(${exec_name} PRIVATE ${WORKFLOW_LIB_DIR})

    target_link_libraries(${exec_name}
        ${COKE_LIBRARY_NAME}
        # use static link, in case not in system path
        -Wl,-Bstatic workflow -Wl,-Bdynamic
        pthread
        OpenSSL::SSL OpenSSL::Crypto
        GTest::GTest GTest::Main
    )

    include_directories(${exec_name}
        ${COKE_INCLUDE_DIR}
        ${WORKFLOW_INCLUDE_DIR}
    )
endforeach()