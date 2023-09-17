config_setting(
    name = "use_gnu_readline",
    values = {
        "define": "use_gnu_readline=true"
    }
)

rl_defs = select({
    "//example:use_gnu_readline": ["USE_GNU_READLINE"],
    "//conditions:default": [],
})

rl_linkopts = select({
    "//example:use_gnu_readline": ["-lreadline"],
    "//conditions:default": [],
})

cc_library(
    name = "readline_helper",
    hdrs = ["readline_helper.h"],
    defines = rl_defs,
    linkopts = rl_linkopts,
)

cc_binary(
    name = "helloworld",
    srcs = ["ex001-helloworld.cpp"],
    deps = [
        "//:common",
        "//:http",
    ],
)

cc_binary(
    name = "http_get",
    srcs = ["ex002-http_get.cpp"],
    deps = [
        "//:common",
        "//:http",
    ],
)

cc_binary(
    name = "http_hello_server",
    srcs = ["ex003-http_hello_server.cpp"],
    deps = [
        "//:common",
        "//:http",
    ],
)

cc_binary(
    name = "word_count",
    srcs = ["ex004-word_count.cpp"],
    deps = [
        "//:common",
    ],
)

cc_binary(
    name = "merge_sort",
    srcs = ["ex005-merge_sort.cpp"],
    deps = [
        "//:common",
    ],
    #linkopts = ["-ltbb"],
)

cc_binary(
    name = "http_proxy",
    srcs = ["ex006-http_proxy.cpp"],
    deps = [
        "//:common",
        "//:http",
    ],
)

cc_binary(
    name = "redis_cli",
    srcs = ["ex007-redis_cli.cpp"],
    deps = [
        "//:common",
        "//:redis",
        "//example:readline_helper"
    ],
)

cc_binary(
    name = "parallel_http_get",
    srcs = ["ex008-parallel_http_get.cpp"],
    deps = [
        "//:common",
        "//:http"
    ],
)

cc_binary(
    name = "mysql_cli",
    srcs = ["ex009-mysql_cli.cpp"],
    deps = [
        "//:common",
        "//:mysql",
        "//example:readline_helper"
    ],
)

cc_binary(
    name = "timed_wait",
    srcs = ["ex010-timed_wait.cpp"],
    deps = [
        "//:common",
    ],
)
