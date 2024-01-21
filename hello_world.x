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
