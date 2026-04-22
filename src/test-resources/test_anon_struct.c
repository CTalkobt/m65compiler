struct S {
    int a;
    struct {
        int b;
        int c;
    };
};

struct S global_s;

int main() {
    global_s.a = 10;
    global_s.b = 20;
    global_s.c = 30;
    
    if (global_s.a != 10) return 1;
    if (global_s.b != 20) return 2;
    if (global_s.c != 30) return 3;
    
    return 0;
}
