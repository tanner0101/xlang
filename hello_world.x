extern fn printf(s: Pointer<UInt8>, ...) -> Int32

fn meaning_of_life() -> Int32 {
    return 42
}

fn main() {
    printf("Hello, world %d!\n", meaning_of_life())
}
