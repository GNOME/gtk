#ifndef MI_H
#define MI_H 1

#include "mitypes.h"
#include "mistruct.h"
#include "mifpoly.h"
#include "mifillarc.h"
#include "mipoly.h"

void miPolyArc(GdkDrawable *pDraw, GdkGC *pGC, int narcs, miArc *parcs);
void miPolyFillArc(GdkDrawable *pDraw, GdkGC *pGC, int narcs, miArc *parcs);
void miFillPolygon(GdkDrawable *dst, GdkGC *pgc, int shape, int mode, int count, GdkPoint *pPts);

miDashPtr miDashLine(int npt, GdkPoint *ppt, unsigned int nDash, unsigned char *pDash, unsigned int offset, int *pnseg);
void miZeroLine(GdkDrawable *pDraw, GdkGC *pGC, int mode, int npt, GdkPoint *pptInit);
void miZeroDashLine(GdkDrawable *dst, GdkGC *pgc, int mode, int nptInit, GdkPoint *pptInit);
void miStepDash (int dist, int *pDashIndex, unsigned char *pDash, int numInDashList, int *pDashOffset);
void miWideDash (GdkDrawable *pDrawable, GdkGC *pGC, int mode, int npt, GdkPoint *pPts);
void miWideLine (GdkDrawable *pDrawable, GdkGC *pGC, int mode, int npt, GdkPoint *pPts);

#endif
