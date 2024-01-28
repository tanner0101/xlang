struct String {
    raw: Pointer<UInt8>
}

extern fn printf(s: Pointer<UInt8>)

fn print(string: String) {
    printf(string.raw)
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
