cmake_minimum_required(VERSION 3.6)

set(ALL_BENCHMARK
    bench_timer
)

foreach(exec_name ${ALL_BENCHMARK})
    add_executable(${exec_name} ${exec_name}.cpp)

    target_link_directories(${exec_name} PRIVATE ${WORKFLOW_LIB_DIR})

    target_link_libraries(${exec_name}
        ${COKE_LIBRARY}
        ${COKE_TRY_STATIC_WORKFLOW}
        Threads::Threads
        OpenSSL::SSL OpenSSL::Crypto
    )
endforeach()