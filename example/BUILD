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
