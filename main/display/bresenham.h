/*
 * bresenham.h
 *
 *  Created on: Mar 3, 2019
 *      Author: Adam Feinstein
 */

#ifndef SRC_BRESENHAM_H_
#define SRC_BRESENHAM_H_

#include "display.h"

typedef int (*translateFn_t)(int);

void oddEvenFill(display_t* disp, int x0, int y0, int x1, int y1,
                 paletteColor_t boundaryColor, paletteColor_t fillColor);

void plotLineScaled(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int dashWidth, int xTr, int yTr, int xScale, int yScale);
void plotLine(display_t* disp, int x0, int y0, int x1, int y1, paletteColor_t col, int dashWidth);
void plotRect(display_t*, int x0, int y0, int x1, int y1, paletteColor_t col);
void plotRectScaled(display_t*, int x0, int y0, int x1, int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotEllipse(display_t*, int xm, int ym, int a, int b, paletteColor_t col);
void plotEllipseScaled(display_t*, int xm, int ym, int a, int b, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotOptimizedEllipse(display_t*, int xm, int ym, int a, int b, paletteColor_t col);
void plotCircle(display_t*, int xm, int ym, int r, paletteColor_t col);
void plotCircleScaled(display_t*, int xm, int ym, int r, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotCircleQuadrants(display_t* disp, int xm, int ym, int r, bool q1,
                         bool q2, bool q3, bool q4, paletteColor_t col);
void plotCircleFilled(display_t* disp, int xm, int ym, int r, paletteColor_t col);
void plotCircleFilledScaled(display_t* disp, int xm, int ym, int r, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotEllipseRect(display_t*, int x0, int y0, int x1, int y1, paletteColor_t col);
void plotEllipseRectScaled(display_t*, int x0, int y0, int x1, int y1, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotQuadBezierSeg(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, paletteColor_t col);
void plotQuadBezierSegScaled(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotQuadBezier(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, paletteColor_t col);
void plotQuadBezierScaled(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotQuadRationalBezierSeg(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, float w, paletteColor_t col);
void plotQuadRationalBezier(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, float w, paletteColor_t col);
void plotRotatedEllipse(display_t*, int x, int y, int a, int b, float angle, paletteColor_t col);
void plotRotatedEllipseRect(display_t*, int x0, int y0, int x1, int y1, long zd, paletteColor_t col);
void plotCubicBezierSeg(display_t*, int x0, int y0, float x1, float y1, float x2, float y2, int x3, int y3,
                        paletteColor_t col);
void plotCubicBezierSegScaled(display_t*, int x0, int y0, float x1, float y1, float x2, float y2, int x3, int y3,
                        paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotCubicBezier(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, paletteColor_t col);
void plotCubicBezierScaled(display_t*, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, paletteColor_t col, int xTr, int yTr, int xScale, int yScale);
void plotQuadSpline(display_t*, int n, int x[], int y[], paletteColor_t col);
void plotCubicSpline(display_t*, int n, int x[], int y[], paletteColor_t col);

#endif /* SRC_BRESENHAM_H_ */
