cmake_minimum_required(VERSION 3.16)

set(ALL_BENCHMARK
    bench_exception
    bench_go
    bench_graph
    bench_lru
    bench_queue
    bench_redis_cli
    bench_timer
    bench_weighted_policy
)

include (../cmake/find-workflow.cmake)

foreach(exec_name ${ALL_BENCHMARK})
    add_executable(${exec_name} ${exec_name}.cpp)

    target_link_directories(${exec_name} PRIVATE ${WORKFLOW_LIB_DIR})

    target_link_libraries(${exec_name}
        coke::coke
        ${COKE_TRY_STATIC_WORKFLOW}
        Threads::Threads
        OpenSSL::SSL OpenSSL::Crypto
    )
endforeach()

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    target_compile_options("bench_graph" PRIVATE "-Wno-unused-comparison")
endif ()
