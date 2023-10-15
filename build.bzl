load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_test")

def create_test_target(name, extra_deps = None):
    if extra_deps == None:
        extra_deps = []

    cc_test(
        name = name,
        srcs = [name + ".cpp"],
        deps = ["//:common"] + extra_deps,
        linkopts = ["-lgtest"],
    )

def create_example_target(example_name, extra_deps = None):
    if extra_deps == None:
        extra_deps = []
    exec_name = example_name.split('-', 1)[1]

    cc_binary(
        name = exec_name,
        srcs = [example_name + ".cpp"],
        deps = ["//:common"] + extra_deps
    )
