int main() {
    int x = 42;
    int *p = &x;
    int **pp = &p;
    
    int y = **pp;
    **pp = 100;
    
    return x;
}
