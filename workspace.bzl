load('@bazel_tools//tools/build_defs/repo:git.bzl', 'git_repository')
def coke_workspace(workflow_tag=None):
    if workflow_tag:
        git_repository(
            name = "workflow",
            remote = "https://github.com/sogou/workflow.git",
            tag = workflow_tag,
        )
