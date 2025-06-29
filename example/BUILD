load("//:build.bzl", "create_example_target")

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

create_example_target("ex001-helloworld", ["//:http"])
create_example_target("ex002-http_get", ["//:http"])
create_example_target("ex003-http_hello_server", ["//:http"])
create_example_target("ex004-word_count")
create_example_target("ex005-merge_sort", ["//:tools"])
create_example_target("ex006-http_proxy", ["//:http"])
create_example_target("ex007-redis_cli", ["//:redis", "//:tools", "//example:readline_helper"])
create_example_target("ex008-parallel_http_get", ["//:http"])
create_example_target("ex009-mysql_cli", ["//:mysql", "//:tools", "//example:readline_helper"])
create_example_target("ex010-timed_wait")
create_example_target("ex011-redis_server", ["//:redis"])
create_example_target("ex012-faster_merge_sort", ["//:tools"])
create_example_target("ex013-background_clock")
create_example_target("ex014-option_parser", ["//:tools"])
create_example_target("ex015-async_queue")
create_example_target("ex016-wait_group")
create_example_target("ex017-backup_request")
create_example_target("ex018-lru_cache")
create_example_target("ex019-task_and_series")
create_example_target("ex020-dag")
create_example_target("ex021-scope_guard", ["//:tools"])
create_example_target("ex022-tlv_server", ["//:tools", "//:tlv"])
create_example_target("ex023-tlv_client", ["//:tools", "//:tlv"])
create_example_target("ex024-tlv_uds", ["//:tlv"])

# virtual target to build all examples
cc_library(
    name = "all_examples",
    deps = [
        ":helloworld",
        ":http_get",
        ":http_hello_server",
        ":word_count",
        ":merge_sort",
        ":http_proxy",
        ":redis_cli",
        ":parallel_http_get",
        ":mysql_cli",
        ":timed_wait",
        ":redis_server",
        ":faster_merge_sort",
        ":background_clock",
        ":option_parser",
        ":async_queue",
        ":wait_group",
        ":backup_request",
        ":lru_cache",
        ":task_and_series",
        ":dag",
        ":scope_guard",
        ":tlv_server",
        ":tlv_client",
        ":tlv_uds",
    ],
)
