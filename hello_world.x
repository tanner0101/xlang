extern fn printf(s: Pointer<UInt8>, ...) -> Int32

fn meaningOfLife() -> Int32 {
    return 42
}

fn main() {
    printf("Hello, world %d!\n", meaningOfLife())
}
