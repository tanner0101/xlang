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
        "//src/parser",
        "//src/parser:node",
        "//src/util:buffer",
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
