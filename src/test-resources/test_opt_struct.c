struct Data {
    char id;
    int value;
    int flags;
};

void main() {
    struct Data d;
    d.id = 1;      // Direct stack access
    d.value = 500; // Direct stack access
    d.flags = d.value; // Optimization: should use register tracking
    
    struct Data* p = &d;
    p->value = 1000; // Fallback to indirect access
}
