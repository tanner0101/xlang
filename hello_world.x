struct Int {
    raw: Int64
}

struct String {
    raw: Pointer<UInt8>
}

extern fn printf(s: Pointer<UInt8>)
extern fn puts(s: Pointer<UInt8>)

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

struct Planet {
    name: String
}

fn main() {
    var earth = Planet("Earth")
    print(erth.name)
    var mars = Planet("Mars")
    greet(mars.name)
}

