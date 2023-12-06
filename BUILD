cc_binary(
    name = "myapp",
    srcs = ["main.cpp"],
)

cc_library(
    name = "token",
    srcs = [
    ],
    hdrs = [
        "token.h",
    ],
)

cc_library(
    name = "lexer",
    srcs = [
        "lexer.cpp",
    ],
    hdrs = [
        "buffer.h",
        "lexer.h",
    ],
    deps = [
        ":token",
    ],
)

cc_library(
    name = "parser",
    srcs = [
        "parser.cpp",
    ],
    hdrs = [
        "ast_node.h",
        "buffer.h",
        "parser.h",
    ],
    deps = [
        ":token",
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

cc_test(
    name = "parser_tests",
    srcs = [
        "parser_tests.cpp",
    ],
    deps = [
        ":parser",
        "@gtest",
        "@gtest//:gtest_main",
    ],
)
