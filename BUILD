package(default_visibility = ["//visibility:public"])

cc_library(
    name = "common",
    srcs = [
        "src/coke_core.cpp",
        "src/fileio.cpp",
        "src/go.cpp",
        "src/sleep.cpp",
        "src/latch.cpp",
        "src/qps_pool.cpp",
    ],
    hdrs = ["include"],
    includes = ["include"],
    deps = [
        "@workflow//:common",
    ]
)

cc_library(
    name = "http",
    srcs = [
        "src/http_impl.cpp"
    ],
    hdrs = ["include"],
    includes = ["include"],
    deps = [
        "//:common",
        "@workflow//:http",
    ],
)

cc_library(
    name = "redis",
    srcs = [
        "src/redis_impl.cpp"
    ],
    hdrs = ["include"],
    includes = ["include"],
    deps = [
        "//:common",
        "@workflow//:redis",
    ],
)

cc_library(
    name = "mysql",
    srcs = [
        "src/mysql_impl.cpp"
    ],
    hdrs = ["include"],
    includes = ["include"],
    deps = [
        "//:common",
        "@workflow//:mysql",
    ],
)
