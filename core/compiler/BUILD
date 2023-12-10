cc_library(
    name = "compiler",
    srcs = [
        "compiler.cpp",
    ],
    hdrs = [
        "compiler.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//core/parser",
        "//core/parser:node",
        "//core/util:buffer",
        "@llvm",
    ],
)

cc_test(
    name = "tests",
    srcs = [
        "compiler_tests.cpp",
    ],
    deps = [
        ":compiler",
        "@gtest",
        "@gtest//:gtest_main",
    ],
)
