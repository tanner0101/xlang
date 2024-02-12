# Setup

Install Bazel: https://bazel.build/install/ubuntu

Get buildifier

```
wget https://github.com/bazelbuild/buildtools/releases/download/v6.4.0/buildifier-linux-amd64
```

Install LLVM: `sudo apt install llvm`

# Example

Compile and run the following program:

```x
extern fn printf(s: Pointer<UInt8>, ...) -> Int32

fn meaning_of_life() -> Int32 {
    return 42
}

fn main() {
    printf("Hello, world %d!\n", meaning_of_life())
}
```

```
cat hello_world.x | bazel run //core:xlang | lli-17
```

# Tests

Run tests using VSCode or `bazel test //...`.

# Clangd

To use clangd extension, install then run:

```
bazel run @hedron_compile_commands//:refresh_all
```