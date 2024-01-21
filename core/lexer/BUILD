cc_library(
    name = "token",
    srcs = [
    ],
    hdrs = [
        "token.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//core/util:enum",
        "//core/util:source",
    ],
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
        "//core/util:diagnostics",
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
