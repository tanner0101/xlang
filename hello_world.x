struct Int {
    raw: Int64
}

struct String {
    raw: Pointer<UInt8>
}

extern fn printf(s: Pointer<UInt8>) -> Int32
extern fn puts(s: Pointer<UInt8>) -> Int32

fn print(string: String) {
    printf(string.raw)
}

struct Planet {
    name: String
    moons: Int
}

fn greet(name: String) {
    print("hello ")
    print(name)
    print("!")
}

fn main() {
    var earth = Planet("Earth", 1)
    print(erth.name)
    var mars = Planet("Mars", 2)
    greet(mars.name)
}

