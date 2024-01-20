fn bar() {
    qux(asdf)
}

fn foo() {
    printf("hello, ")
    bar()
}

fn main() {
    foo()
    printf("world!")
}
