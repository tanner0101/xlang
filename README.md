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
fn main() {
    print("Hello, world!")
}
```

```
cat hello_world.x | bazel run //src:xlang | lli
```

# Tests

Run tests using VSCode or `bazel test //...`.
