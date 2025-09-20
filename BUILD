package(default_visibility = ["//visibility:public"])

cc_library(
    name = "detail_hdrs",
    hdrs = glob(["include/coke/detail/*.h"])
)

cc_library(
    name = "compatible_hdrs",
    hdrs = glob(["include/coke/compatible/*.h"])
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
        "src/rbtree.cpp",
        "src/sleep.cpp",
        "src/stop_token.cpp",
        "src/str_packer.cpp",
        "src/sync_guard.cpp",
    ],
    hdrs = [
        "include/coke/basic_awaiter.h",
        "include/coke/coke.h",
        "include/coke/condition.h",
        "include/coke/dag.h",
        "include/coke/deque.h",
        "include/coke/fileio.h",
        "include/coke/future.h",
        "include/coke/global.h",
        "include/coke/go.h",
        "include/coke/latch.h",
        "include/coke/lru_cache.h",
        "include/coke/make_task.h",
        "include/coke/mutex.h",
        "include/coke/qps_pool.h",
        "include/coke/queue_common.h",
        "include/coke/queue.h",
        "include/coke/semaphore.h",
        "include/coke/series.h",
        "include/coke/shared_mutex.h",
        "include/coke/sleep.h",
        "include/coke/stop_token.h",
        "include/coke/sync_guard.h",
        "include/coke/task.h",
        "include/coke/wait_group.h",
        "include/coke/wait.h",

        "include/coke/utils/list.h",
        "include/coke/utils/rbtree.h",
        "include/coke/utils/str_holder.h",
        "include/coke/utils/str_packer.h",
    ],
    includes = ["include"],
    deps = [
        "//:detail_hdrs",
        "//:compatible_hdrs",
        "@workflow//:common",
    ]
)

cc_library(
    name = "nspolicy",
    srcs = [
        "src/nspolicy/nspolicy.cpp",
        "src/nspolicy/weighted_random_policy.cpp",
        "src/nspolicy/weighted_round_robin_policy.cpp",
        "src/nspolicy/weighted_least_conn_policy.cpp",
    ],
    hdrs = glob(["include/coke/nspolicy/*.h"]),
    includes = ["include"],
    deps = [
        "//:common",
    ],
)

cc_library(
    name = "net",
    srcs = [
        "src/net/client_conn_info.cpp",
    ],
    hdrs = glob(["include/coke/net/*.h"]),
    includes = ["include"],
    deps = [
        "//:common",
    ],
)

cc_library(
    name = "tlv",
    srcs = [
        "src/net/tlv_task.cpp",
        "src/net/tlv_client.cpp",
    ],
    hdrs = glob(["include/coke/tlv/*.h"]),
    includes = ["include"],
    deps = [
        "//:net",
    ],
)

cc_library(
    name = "http",
    srcs = [
        "src/http_impl.cpp"
    ],
    hdrs = glob(["include/coke/http/*.h"]),
    includes = ["include"],
    deps = [
        "//:net",
        "@workflow//:http",
    ],
)

cc_library(
    name = "redis",
    srcs = [
        "src/redis/client_task.cpp",
        "src/redis/client.cpp",
        "src/redis/cluster_client.cpp",
        "src/redis/message.cpp",
        "src/redis/parser.cpp",
        "src/redis/value.cpp",
    ],
    hdrs = [
        "include/coke/redis/basic_types.h",
        "include/coke/redis/client_impl.h",
        "include/coke/redis/client_task.h",
        "include/coke/redis/client.h",
        "include/coke/redis/cluster_client_impl.h",
        "include/coke/redis/cluster_client.h",
        "include/coke/redis/message.h",
        "include/coke/redis/options.h",
        "include/coke/redis/parser.h",
        "include/coke/redis/server.h",
        "include/coke/redis/value.h",

        "include/coke/redis/commands/bitmap.h",
        "include/coke/redis/commands/generic.h",
        "include/coke/redis/commands/hash.h",
        "include/coke/redis/commands/list.h",
        "include/coke/redis/commands/string.h",
    ],
    includes = ["include"],
    deps = [
        "//:net",
    ],
)

cc_library(
    name = "mysql",
    srcs = [
        "src/mysql_impl.cpp"
    ],
    hdrs = glob(["include/coke/mysql/*.h"]),
    includes = ["include"],
    deps = [
        "//:net",
        "@workflow//:mysql",
    ],
)
