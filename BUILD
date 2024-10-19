package(default_visibility = ["//visibility:public"])

cc_library(
    name = "detail_hdrs",
    hdrs = glob(["include/coke/detail/*"])
)

cc_library(
    name = "compatible_hdrs",
    hdrs = glob(["include/coke/compatible/*"])
)

cc_library(
    name = "tools",
    hdrs = glob(["include/coke/tools/*.h"]),
    includes = ["include"],
)

cc_library(
    name = "common",
    srcs = [
        "src/cancelable_timer.cpp",
        "src/coke_impl.cpp",
        "src/condition.cpp",
        "src/dag.cpp",
        "src/fileio.cpp",
        "src/go.cpp",
        "src/latch.cpp",
        "src/mutex.cpp",
        "src/qps_pool.cpp",
        "src/random.cpp",
        "src/sleep.cpp",
        "src/stop_token.cpp",
    ],
    hdrs = [
        "include/coke/basic_awaiter.h",
        "include/coke/basic_server.h",
        "include/coke/coke.h",
        "include/coke/condition.h",
        "include/coke/dag.h",
        "include/coke/deque.h",
        "include/coke/fileio.h",
        "include/coke/future.h",
        "include/coke/global.h",
        "include/coke/go.h",
        "include/coke/latch.h",
        "include/coke/make_task.h",
        "include/coke/mutex.h",
        "include/coke/network.h",
        "include/coke/qps_pool.h",
        "include/coke/queue_common.h",
        "include/coke/queue.h",
        "include/coke/semaphore.h",
        "include/coke/series.h",
        "include/coke/shared_mutex.h",
        "include/coke/sleep.h",
        "include/coke/stop_token.h",
        "include/coke/task.h",
        "include/coke/wait_group.h",
        "include/coke/wait.h",
    ],
    includes = ["include"],
    deps = [
        "//:detail_hdrs",
        "//:compatible_hdrs",
        "@workflow//:common",
    ]
)

cc_library(
    name = "http",
    srcs = [
        "src/http_impl.cpp"
    ],
    hdrs = [
        "include/coke/http_client.h",
        "include/coke/http_server.h",
        "include/coke/http_utils.h",
    ],
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
    hdrs = [
        "include/coke/redis_client.h",
        "include/coke/redis_server.h",
        "include/coke/redis_utils.h",
    ],
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
    hdrs = [
        "include/coke/mysql_client.h",
        "include/coke/mysql_utils.h",
    ],
    includes = ["include"],
    deps = [
        "//:common",
        "@workflow//:mysql",
    ],
)
