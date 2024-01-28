struct String {
    pointer: Pointer<UInt8>
}

extern fn printf(string: Pointer<UInt8>)

fn print(string: String) {
    printf(string.pointer)
}

struct Planet {
    name: String
}

fn greet(name: String) {
    print("hello ")
    print(name)
    print("!")
}

fn main() {
    var name = "world"
    greet(name)
    greet("tanner")
    greet(42)
    greet()
}
