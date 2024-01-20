load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "gtest",
    strip_prefix = "googletest-5ab508a01f9eb089207ee87fd547d290da39d015",
    urls = ["https://github.com/google/googletest/archive/5ab508a01f9eb089207ee87fd547d290da39d015.zip"],
)

new_local_repository(
    name = "llvm",
    build_file_content = """
cc_library(
    name = "llvm",
    includes = ["."],
    hdrs = glob(["**/*.h", "**/*.def", "**/*.inc", "**/*.td", "**/*.gen"]),
    linkopts = [
        "-L/usr/lib/llvm-14/lib",
        "-lLLVM",
    ],
    visibility = ["//visibility:public"]
)
""",
    path = "/usr/lib/llvm-14/include",
)

new_local_repository(
    name = "boost",
    build_file_content = """
cc_library(
    name = "boost",
    hdrs = glob(["boost/**/*.hpp"]),
    includes = ["."],
    linkopts = [
        "-L/usr/local/lib",
        "-lboost_system",
        "-lboost_json",
    ],
    visibility = ["//visibility:public"]
)
""",
    path = "/usr/local/include",
)
