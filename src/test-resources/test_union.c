union U {
    int a;
    char b;
};

struct S {
    int x;
    union {
        int y;
        char z;
    };
};

union U global_u;
struct S global_s;

int main() {
    global_u.a = 0x1234;
    if (global_u.b != 0x34) return 1;
    
    global_s.x = 100;
    global_s.y = 0x5678;
    if (global_s.z != 0x78) return 2;
    
    return 0;
}
