#pragma once

typedef struct Poly Poly;

Poly  *poly_new    (int n, double *c);
void   poly_free   (Poly *p);
double poly_eval   (const Poly *p, double t);
Poly  *poly_bezier (int n, double *w);
char  *poly_to_string (const Poly *p);

double poly_curve_find_closest_point (Poly *qx, Poly *qy,
                                      double px, double py);

