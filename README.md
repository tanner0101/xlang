# Setup

Install Bazel: https://bazel.build/install/ubuntu

Get buildifier

```
wget https://github.com/bazelbuild/buildtools/releases/download/v6.4.0/buildifier-linux-amd64
```

# Lexer

Build

```
bazel build //:lexer
```

Test

```
bazel run //:lexer_tests
```
