load("//:build.bzl", "create_benchmark_target")

cc_library(
    name = "option_parser",
    hdrs = [
        "option_parser.h",
    ],
)

cc_library(
    name = "bench_common",
    hdrs = [
        "bench_common.h",
    ],
)

create_benchmark_target("bench_timer")
create_benchmark_target("bench_go")
create_benchmark_target("bench_graph")
create_benchmark_target("bench_exception")
