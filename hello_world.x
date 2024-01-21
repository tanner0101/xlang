fn greet(name: String) {
    printf("hello ")
    printf(name)
    printf("!")
}

fn main() {
    var name = "world"
    greet(name)
    greet("tanner")
    greet()
}
