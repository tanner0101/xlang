cc_library(
    name = "node",
    hdrs = [
        "node.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//core/lexer:token",
        "//core/util:buffer",
        "//core/util:enum",
    ],
)

cc_library(
    name = "parser",
    srcs = [
        "parser.cpp",
    ],
    hdrs = [
        "parser.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":node",
        "//core/lexer:token",
        "//core/util:buffer",
        "//core/util:diagnostics",
    ],
)

cc_test(
    name = "tests",
    srcs = [
        "parser_tests.cpp",
    ],
    deps = [
        ":parser",
        "@gtest",
        "@gtest//:gtest_main",
    ],
)
