load("//:build.bzl", "create_benchmark_target")

cc_library(
    name = "bench_common",
    hdrs = [
        "bench_common.h",
    ],
    deps = ["//:tools", "//:common"],
)

create_benchmark_target("bench_exception")
create_benchmark_target("bench_go")
create_benchmark_target("bench_graph")
create_benchmark_target("bench_queue")
create_benchmark_target("bench_timer")
