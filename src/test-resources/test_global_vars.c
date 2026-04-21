int g_x = 42;
char g_c = 10;
int g_y;

int main() {
    if (g_x != 42) return 1;
    if (g_c != 10) return 2;
    
    g_y = g_x + g_c;
    if (g_y != 52) return 3;
    
    g_x = 100;
    if (g_x != 100) return 4;
    
    return 0;
}
