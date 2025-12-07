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

    target_link_libraries(${exec_name}
        PRIVATE
            coke::coke
            coke-workflow
    )
endforeach()
