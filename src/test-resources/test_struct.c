struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point topLeft;
    struct Point bottomRight;
};

int main() {
    struct Point p;
    p.x = 10;
    p.y = 20;
    
    struct Point *pp = &p;
    pp->x = 30;
    
    struct Rect r;
    r.topLeft.x = 100;
    r.bottomRight.y = 200;
    
    return p.x + p.y + r.topLeft.x;
}
