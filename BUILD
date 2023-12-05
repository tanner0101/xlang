cc_binary(
    name = "myapp",
    srcs = ["main.cpp"],
)

cc_library(
    name = "lexer",
    srcs = [
        "lexer.cpp",
    ],
    hdrs = [
        "lexer.h",
    ],
)

cc_test(
    name = "lexer_tests",
    srcs = [
        "lexer_tests.cpp",
    ],
    deps = [
        ":lexer",
        "@gtest",
        "@gtest//:gtest_main",
    ],
)
