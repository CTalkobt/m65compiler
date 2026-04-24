typedef int my_int;
typedef char* string;
typedef struct {
    int x;
    int y;
} Point;

typedef my_int alias_int;

int main() {
    my_int a = 10;
    alias_int b = 20;
    string s = "hello";
    Point p;
    p.x = a;
    p.y = b;

    typedef int local_int;
    local_int c = 30;

    if (a + b + c == 60) {
        return 0; // PASS
    }
    return 1; // FAIL
}
