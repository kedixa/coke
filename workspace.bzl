load('@bazel_tools//tools/build_defs/repo:git.bzl', 'git_repository')

def coke_workspace(workflow_tag=None, workflow_commit=None, workflow_path=None):
    if workflow_path:
        native.local_repository(
            name = "workflow",
            path = workflow_path
        )
    elif workflow_tag or workflow_commit:
        git_repository(
            name = "workflow",
            remote = "https://github.com/sogou/workflow.git",
            tag = workflow_tag,
            commit = workflow_commit
        )
