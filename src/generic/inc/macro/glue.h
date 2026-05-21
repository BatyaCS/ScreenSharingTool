#ifndef GLUE

#define _GLUE(x, y) x ## y
#define GLUE(x, y) _GLUE(x, y)
#define GLUE2(a, b) _GLUE(a, b)
#define GLUE3(a, b, c) GLUE(GLUE2(a, b), c)
#define GLUE4(a, b, c, d) GLUE(GLUE3(a, b, c), d)
#define GLUE5(a, b, c, d, e) GLUE(GLUE4(a, b, c, d), e)
#endif /* GLUE */