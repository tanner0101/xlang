cc_library(
    name = "token",
    srcs = [
    ],
    hdrs = [
        "token.h",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "lexer",
    srcs = [
        "lexer.cpp",
    ],
    hdrs = [
        "lexer.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":token",
        "//core/util:buffer",
    ],
)

cc_test(
    name = "tests",
    srcs = [
        "lexer_tests.cpp",
    ],
    deps = [
        ":lexer",
        "@gtest",
        "@gtest//:gtest_main",
    ],
)
