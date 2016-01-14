/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -  This software is distributed in the hope that it will be
 -  useful, but with NO WARRANTY OF ANY KIND.
 -  No author or distributor accepts responsibility to anyone for the
 -  consequences of using this software, or for whether it serves any
 -  particular purpose or works at all, unless he or she says so in
 -  writing.  Everyone is granted permission to copy, modify and
 -  redistribute this source code, for commercial or non-commercial
 -  purposes, with the following restrictions: (1) the origin of this
 -  source code must not be misrepresented; (2) modified versions must
 -  be plainly marked as such; and (3) this notice may not be removed
 -  or altered from any source or modified source distribution.
 *====================================================================*/

/*
 *  projective.c
 *
 *      Projective (4 pt) image transformation using a sampled
 *      (to nearest integer) transform on each dest point
 *           PIX      *pixProjectiveSampledPta()
 *           PIX      *pixProjectiveSampled()
 *
 *      Projective (4 pt) image transformation using interpolation 
 *      (or area mapping) for anti-aliasing images that are
 *      2, 4, or 8 bpp gray, or colormapped, or 32 bpp RGB
 *           PIX      *pixProjectivePta()
 *           PIX      *pixProjective()
 *           PIX      *pixProjectivePtaColor()
 *           PIX      *pixProjectiveColor()
 *           PIX      *pixProjectivePtaGray()
 *           PIX      *pixProjectiveGray()
 *
 *      Projective coordinate transformation
 *           l_int32   getProjectiveXformCoeffs()
 *           l_int32   projectiveXformSampledPt()
 *           l_int32   projectiveXformPt()
 *
 *      A projective transform can be specified as a specific functional
 *      mapping between 4 points in the source and 4 points in the dest.
 *      It preserves straight lines, but is less stable than a bilinear
 *      transform, because it contains a division by a quantity that
 *      can get arbitrarily small.)
 *
 *      We give both a projective coordinate transformation and
 *      two projective image transformations.
 *
 *      For the former, we ask for the coordinate value (x',y')
 *      in the transformed space for any point (x,y) in the original
 *      space.  The coefficients of the transformation are found by
 *      solving 8 simultaneous equations for the 8 coordinates of
 *      the 4 points in src and dest.  The transformation can then
 *      be used to compute the associated image transform, by
 *      computing, for each dest pixel, the relevant pixel(s) in
 *      the source.  This can be done either by taking the closest
 *      src pixel to each transformed dest pixel ("sampling") or
 *      by doing an interpolation and averaging over 4 source
 *      pixels with appropriate weightings ("interpolated").
 *
 *      A typical application would be to remove keystoning
 *      due to a projective transform in the imaging system.
 *
 *      The projective transform is given by specifying two equations:
 *
 *          x' = (ax + by + c) / (gx + hy + 1)
 *          y' = (dx + ey + f) / (gx + hy + 1)
 *
 *      where the eight coefficients have been computed from four
 *      sets of these equations, each for two corresponding data pts.
 *      In practice, for each point (x,y) in the dest image, this
 *      equation is used to compute the corresponding point (x',y')
 *      in the src.  That computed point in the src is then used
 *      to determine the dest value in one of two ways:
 *
 *       - sampling: take the value of the src pixel in which this
 *                   point falls
 *       - interpolation: take appropriate linear combinations of the
 *                        four src pixels that this dest pixel would
 *                        overlap, with the coefficients proportional
 *                        to the amount of overlap
 *
 *      For small warp where there is little scale change, (e.g.,
 *      for rotation) area mapping is nearly equivalent to interpolation.
 *
 *      Typical relative timing of pointwise transforms (sampled = 1.0):
 *      8 bpp:   sampled        1.0
 *               interpolated   1.5
 *      32 bpp:  sampled        1.0
 *               interpolated   1.6
 *      Additionally, the computation time/pixel is nearly the same
 *      for 8 bpp and 32 bpp, for both sampled and interpolated.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "allheaders.h"


/*-------------------------------------------------------------*
 *            Sampled projective image transformation          *
 *-------------------------------------------------------------*/
/*!
 *  pixProjectiveSampledPta()
 *
 *      Input:  pixs (all depths)
 *              ptad  (4 pts of final coordinate space)
 *              ptas  (4 pts of initial coordinate space)
 *              incolor (L_BRING_IN_WHITE, L_BRING_IN_BLACK)
 *      Return: pixd, or null on error
 *
 *  Notes:
 *      (1) Brings in either black or white pixels from the boundary.
 *      (2) Retains colormap, which you can do for a sampled transform..
 *      (3) No 3 of the 4 points may be collinear.
 *      (4) For 8 and 32 bpp pix, better quality is obtained by the
 *          somewhat slower pixProjectivePta().  See that
 *          function for relative timings between sampled and interpolated.
 */
PIX *
pixProjectiveSampledPta(PIX     *pixs,
                        PTA     *ptad,
                        PTA     *ptas,
                        l_int32  incolor)
{
l_float32  *vc;
PIX        *pixd;

    PROCNAME("pixProjectiveSampledPta");

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (!ptas)
        return (PIX *)ERROR_PTR("ptas not defined", procName, NULL);
    if (!ptad)
        return (PIX *)ERROR_PTR("ptad not defined", procName, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)ERROR_PTR("invalid incolor", procName, NULL);
    if (ptaGetCount(ptas) != 4)
        return (PIX *)ERROR_PTR("ptas count not 4", procName, NULL);
    if (ptaGetCount(ptad) != 4)
        return (PIX *)ERROR_PTR("ptad count not 4", procName, NULL);

        /* Get backwards transform from dest to src, and apply it */
    getProjectiveXformCoeffs(ptad, ptas, &vc);
    pixd = pixProjectiveSampled(pixs, vc, incolor);
    FREE(vc);

    return pixd;
}


/*!
 *  pixProjectiveSampled()
 *
 *      Input:  pixs (all depths)
 *              vc  (vector of 8 coefficients for projective transformation)
 *              incolor (L_BRING_IN_WHITE, L_BRING_IN_BLACK)
 *      Return: pixd, or null on error
 *
 *  Notes:
 *      (1) Brings in either black or white pixels from the boundary.
 *      (2) Retains colormap, which you can do for a sampled transform..
 *      (3) For 8 or 32 bpp, much better quality is obtained by the
 *          somewhat slower pixProjective().  See that function
 *          for relative timings between sampled and interpolated.
 */
PIX *
pixProjectiveSampled(PIX        *pixs,
                     l_float32  *vc,
                     l_int32     incolor)
{
l_int32     i, j, w, h, d, x, y, wpls, wpld, color, cmapindex;
l_uint32    val;
l_uint32   *datas, *datad, *lines, *lined;
PIX        *pixd;
PIXCMAP    *cmap;

    PROCNAME("pixProjectiveSampled");

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (!vc)
        return (PIX *)ERROR_PTR("vc not defined", procName, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)ERROR_PTR("invalid incolor", procName, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 32)
        return (PIX *)ERROR_PTR("depth not 1, 2, 4, 8 or 16", procName, NULL);

        /* Init all dest pixels to color to be brought in from outside */
    pixd = pixCreateTemplate(pixs);
    if ((cmap = pixGetColormap(pixs)) != NULL) {
        if (incolor == L_BRING_IN_WHITE)
            color = 1;
        else
            color = 0;
        pixcmapAddBlackOrWhite(cmap, color, &cmapindex);
        pixSetAllArbitrary(pixd, cmapindex);
    }
    else {
        if ((d == 1 && incolor == L_BRING_IN_WHITE) ||
            (d > 1 && incolor == L_BRING_IN_BLACK))
            pixClearAll(pixd);
        else
            pixSetAll(pixd);
    }

        /* Scan over the dest pixels */
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            projectiveXformSampledPt(vc, j, i, &x, &y);
            if (x < 0 || y < 0 || x >=w || y >= h)
                continue;
            lines = datas + y * wpls;
            if (d == 1) {
                if (GET_DATA_BIT(lines, x))
                    SET_DATA_BIT(lined, j);
            }
            else if (d == 8) {
                val = GET_DATA_BYTE(lines, x);
                SET_DATA_BYTE(lined, j, val);
            }
            else if (d == 32) {
                lined[j] = lines[x];
            }
            else if (d == 2) {
                val = GET_DATA_DIBIT(lines, x);
                SET_DATA_DIBIT(lined, j, val);
            }
            else if (d == 4) {
                val = GET_DATA_QBIT(lines, x);
                SET_DATA_QBIT(lined, j, val);
            }
        }
    }

    return pixd;
}


/*---------------------------------------------------------------------*
 *            Interpolated projective image transformation             *
 *---------------------------------------------------------------------*/
/*!
 *  pixProjectivePta()
 *
 *      Input:  pixs (all depths; colormap ok)
 *              ptad  (4 pts of final coordinate space)
 *              ptas  (4 pts of initial coordinate space)
 *              incolor (L_BRING_IN_WHITE, L_BRING_IN_BLACK)
 *      Return: pixd, or null on error
 *
 *  Notes:
 *      (1) Brings in either black or white pixels from the boundary
 *      (2) Removes any existing colormap, if necessary, before transforming
 */
PIX *
pixProjectivePta(PIX     *pixs,
                 PTA     *ptad,
                 PTA     *ptas,
                 l_int32  incolor)
{
l_int32   d;
l_uint32  colorval;
PIX      *pixt1, *pixt2, *pixd;

    PROCNAME("pixProjectivePta");

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (!ptas)
        return (PIX *)ERROR_PTR("ptas not defined", procName, NULL);
    if (!ptad)
        return (PIX *)ERROR_PTR("ptad not defined", procName, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)ERROR_PTR("invalid incolor", procName, NULL);
    if (ptaGetCount(ptas) != 4)
        return (PIX *)ERROR_PTR("ptas count not 4", procName, NULL);
    if (ptaGetCount(ptad) != 4)
        return (PIX *)ERROR_PTR("ptad count not 4", procName, NULL);

    if (pixGetDepth(pixs) == 1)
        return pixProjectiveSampledPta(pixs, ptad, ptas, incolor);

        /* Remove cmap if it exists, and unpack to 8 bpp if necessary */
    pixt1 = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    d = pixGetDepth(pixt1);
    if (d < 8)
        pixt2 = pixConvertTo8(pixt1, FALSE);
    else
        pixt2 = pixClone(pixt1);
    d = pixGetDepth(pixt2);

        /* Compute actual color to bring in from edges */
    colorval = 0;
    if (incolor == L_BRING_IN_WHITE) {
        if (d == 8)
            colorval = 255;
        else  /* d == 32 */
            colorval = 0xffffff00;
    }
    
    if (d == 8)
        pixd = pixProjectivePtaGray(pixt2, ptad, ptas, colorval);
    else  /* d == 32 */
        pixd = pixProjectivePtaColor(pixt2, ptad, ptas, colorval);
    pixDestroy(&pixt1);
    pixDestroy(&pixt2);
    return pixd;
}


/*!
 *  pixProjective()
 *
 *      Input:  pixs (all depths; colormap ok)
 *              vc  (vector of 8 coefficients for affine transformation)
 *              incolor (L_BRING_IN_WHITE, L_BRING_IN_BLACK)
 *      Return: pixd, or null on error
 *
 *  Notes:
 *      (1) Brings in either black or white pixels from the boundary
 *      (2) Removes any existing colormap, if necessary, before transforming
 */
PIX *
pixProjective(PIX        *pixs,
              l_float32  *vc,
              l_int32     incolor)
{
l_int32   d;
l_uint32  colorval;
PIX      *pixt1, *pixt2, *pixd;

    PROCNAME("pixProjective");

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (!vc)
        return (PIX *)ERROR_PTR("vc not defined", procName, NULL);

    if (pixGetDepth(pixs) == 1)
        return pixProjectiveSampled(pixs, vc, incolor);

        /* Remove cmap if it exists, and unpack to 8 bpp if necessary */
    pixt1 = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    d = pixGetDepth(pixt1);
    if (d < 8)
        pixt2 = pixConvertTo8(pixt1, FALSE);
    else
        pixt2 = pixClone(pixt1);
    d = pixGetDepth(pixt2);

        /* Compute actual color to bring in from edges */
    colorval = 0;
    if (incolor == L_BRING_IN_WHITE) {
        if (d == 8)
            colorval = 255;
        else  /* d == 32 */
            colorval = 0xffffff00;
    }
    
    if (d == 8)
        pixd = pixProjectiveGray(pixt2, vc, colorval);
    else  /* d == 32 */
        pixd = pixProjectiveColor(pixt2, vc, colorval);
    pixDestroy(&pixt1);
    pixDestroy(&pixt2);
    return pixd;
}


/*!
 *  pixProjectivePtaColor()
 *
 *      Input:  pixs (32 bpp)
 *              ptad  (4 pts of final coordinate space)
 *              ptas  (4 pts of initial coordinate space)
 *              colorval (e.g., 0 to bring in BLACK, 0xffffff00 for WHITE)
 *      Return: pixd, or null on error
 */
PIX *
pixProjectivePtaColor(PIX      *pixs,
                      PTA      *ptad,
                      PTA      *ptas,
                      l_uint32  colorval)
{
l_float32  *vc;
PIX        *pixd;

    PROCNAME("pixProjectivePtaColor");

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (!ptas)
        return (PIX *)ERROR_PTR("ptas not defined", procName, NULL);
    if (!ptad)
        return (PIX *)ERROR_PTR("ptad not defined", procName, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs must be 32 bpp", procName, NULL);
    if (ptaGetCount(ptas) != 4)
        return (PIX *)ERROR_PTR("ptas count not 4", procName, NULL);
    if (ptaGetCount(ptad) != 4)
        return (PIX *)ERROR_PTR("ptad count not 4", procName, NULL);

        /* Get backwards transform from dest to src, and apply it */
    getProjectiveXformCoeffs(ptad, ptas, &vc);
    pixd = pixProjectiveColor(pixs, vc, colorval);
    FREE(vc);

    return pixd;
}


/*!
 *  pixProjectiveColor()
 *
 *      Input:  pixs (32 bpp)
 *              vc  (vector of 6 coefficients for affine transformation)
 *              colorval (e.g., 0 to bring in BLACK, 0xffffff00 for WHITE)
 *      Return: pixd, or null on error
 */
PIX *
pixProjectiveColor(PIX        *pixs,
                   l_float32  *vc,
                   l_uint32    colorval)
{
l_int32    i, j, w, h, d, wpls, wpld;
l_uint32   val;
l_uint32  *datas, *datad, *lined;
l_float32  x, y;
PIX       *pixd;

    PROCNAME("pixProjectiveColor");

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 32)
        return (PIX *)ERROR_PTR("pixs must be 32 bpp", procName, NULL);
    if (!vc)
        return (PIX *)ERROR_PTR("vc not defined", procName, NULL);

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    pixd = pixCreateTemplate(pixs);
    pixSetAllArbitrary(pixd, colorval);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* Iterate over destination pixels */
    for (i = 0; i < h; i++) {
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
                /* Compute float src pixel location corresponding to (i,j) */
            projectiveXformPt(vc, j, i, &x, &y);
            linearInterpolatePixelColor(datas, wpls, w, h, x, y, colorval,
                                        &val);
            *(lined + j) = val;
        }
    }

    return pixd;
}


/*!
 *  pixProjectivePtaGray()
 *
 *      Input:  pixs (8 bpp)
 *              ptad  (4 pts of final coordinate space)
 *              ptas  (4 pts of initial coordinate space)
 *              grayval (0 to bring in BLACK, 255 for WHITE)
 *      Return: pixd, or null on error
 */
PIX *
pixProjectivePtaGray(PIX     *pixs,
                     PTA     *ptad,
                     PTA     *ptas,
                     l_uint8  grayval)
{
l_float32  *vc;
PIX        *pixd;

    PROCNAME("pixProjectivePtaGray");

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (!ptas)
        return (PIX *)ERROR_PTR("ptas not defined", procName, NULL);
    if (!ptad)
        return (PIX *)ERROR_PTR("ptad not defined", procName, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs must be 8 bpp", procName, NULL);
    if (ptaGetCount(ptas) != 4)
        return (PIX *)ERROR_PTR("ptas count not 4", procName, NULL);
    if (ptaGetCount(ptad) != 4)
        return (PIX *)ERROR_PTR("ptad count not 4", procName, NULL);

        /* Get backwards transform from dest to src, and apply it */
    getProjectiveXformCoeffs(ptad, ptas, &vc);
    pixd = pixProjectiveGray(pixs, vc, grayval);
    FREE(vc);

    return pixd;
}



/*!
 *  pixProjectiveGray()
 *
 *      Input:  pixs (8 bpp)
 *              vc  (vector of 8 coefficients for affine transformation)
 *              grayval (0 to bring in BLACK, 255 for WHITE)
 *      Return: pixd, or null on error
 */
PIX *
pixProjectiveGray(PIX        *pixs,
                  l_float32  *vc,
                  l_uint8     grayval)
{
l_int32    i, j, w, h, wpls, wpld, val;
l_uint32  *datas, *datad, *lined;
l_float32  x, y;
PIX       *pixd;

    PROCNAME("pixProjectiveGray");

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs must be 8 bpp", procName, NULL);
    if (!vc)
        return (PIX *)ERROR_PTR("vc not defined", procName, NULL);

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    pixd = pixCreateTemplate(pixs);
    pixSetAllArbitrary(pixd, grayval);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* Iterate over destination pixels */
    for (i = 0; i < h; i++) {
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
                /* Compute float src pixel location corresponding to (i,j) */
            projectiveXformPt(vc, j, i, &x, &y);
            linearInterpolatePixelGray(datas, wpls, w, h, x, y, grayval, &val);
            SET_DATA_BYTE(lined, j, val);
        }
    }

    return pixd;
}


/*-------------------------------------------------------------*
 *                Projective coordinate transformation         *
 *-------------------------------------------------------------*/
/*!
 *  getProjectiveXformCoeffs()
 *
 *      Input:  ptas  (source 4 points; unprimed)
 *              ptad  (transformed 4 points; primed)
 *              &vc   (<return> vector of coefficients of transform)
 *      Return: 0 if OK; 1 on error
 *
 *  We have a set of 8 equations, describing the projective
 *  transformation that takes 4 points (ptas) into 4 other
 *  points (ptad).  These equations are:
 *
 *          x1' = (c[0]*x1 + c[1]*y1 + c[2]) / (c[6]*x1 + c[7]*y1 + 1)
 *          y1' = (c[3]*x1 + c[4]*y1 + c[5]) / (c[6]*x1 + c[7]*y1 + 1)
 *          x2' = (c[0]*x2 + c[1]*y2 + c[2]) / (c[6]*x2 + c[7]*y2 + 1)
 *          y2' = (c[3]*x2 + c[4]*y2 + c[5]) / (c[6]*x2 + c[7]*y2 + 1)
 *          x3' = (c[0]*x3 + c[1]*y3 + c[2]) / (c[6]*x3 + c[7]*y3 + 1)
 *          y3' = (c[3]*x3 + c[4]*y3 + c[5]) / (c[6]*x3 + c[7]*y3 + 1)
 *          x4' = (c[0]*x4 + c[1]*y4 + c[2]) / (c[6]*x4 + c[7]*y4 + 1)
 *          y4' = (c[3]*x4 + c[4]*y4 + c[5]) / (c[6]*x4 + c[7]*y4 + 1)
 *    
 *  Multiplying both sides of each eqn by the denominator, we get
 *
 *           AC = B
 *
 *  where B and C are column vectors
 *    
 *         B = [ x1' y1' x2' y2' x3' y3' x4' y4' ]
 *         C = [ c[0] c[1] c[2] c[3] c[4] c[5] c[6] c[7] ]
 *    
 *  and A is the 8x8 matrix
 *
 *             x1   y1     1     0   0    0   -x1*x1'  -y1*x1'
 *              0    0     0    x1   y1   1   -x1*y1'  -y1*y1'
 *             x2   y2     1     0   0    0   -x2*x2'  -y2*x2'
 *              0    0     0    x2   y2   1   -x2*y2'  -y2*y2'
 *             x3   y3     1     0   0    0   -x3*x3'  -y3*x3'
 *              0    0     0    x3   y3   1   -x3*y3'  -y3*y3'
 *             x4   y4     1     0   0    0   -x4*x4'  -y4*x4'
 *              0    0     0    x4   y4   1   -x4*y4'  -y4*y4'
 *
 *  These eight equations are solved here for the coefficients C.
 *
 *  These eight coefficients can then be used to find the mapping
 *  (x,y) --> (x',y'):
 *
 *           x' = (c[0]x + c[1]y + c[2]) / (c[6]x + c[7]y + 1)
 *           y' = (c[3]x + c[4]y + c[5]) / (c[6]x + c[7]y + 1)
 *
 *  that is implemented in projectiveXformSampled() and
 *  projectiveXFormInterpolated().
 */
l_int32
getProjectiveXformCoeffs(PTA         *ptas,
                         PTA         *ptad,
                         l_float32  **pvc)
{
l_int32     i;
l_float32   x1, y1, x2, y2, x3, y3, x4, y4;
l_float32  *b;   /* rhs vector of primed coords X'; coeffs returned in *pvc */
l_float32  *a[8];  /* 8x8 matrix A  */

    PROCNAME("getProjectiveXformCoeffs");

    if (!ptas)
        return ERROR_INT("ptas not defined", procName, 1);
    if (!ptad)
        return ERROR_INT("ptad not defined", procName, 1);
    if (!pvc)
        return ERROR_INT("&vc not defined", procName, 1);
        
    if ((b = (l_float32 *)CALLOC(8, sizeof(l_float32))) == NULL)
        return ERROR_INT("b not made", procName, 1);
    *pvc = b;

    ptaGetPt(ptas, 0, &x1, &y1);
    ptaGetPt(ptas, 1, &x2, &y2);
    ptaGetPt(ptas, 2, &x3, &y3);
    ptaGetPt(ptas, 3, &x4, &y4);
    ptaGetPt(ptad, 0, &b[0], &b[1]);
    ptaGetPt(ptad, 1, &b[2], &b[3]);
    ptaGetPt(ptad, 2, &b[4], &b[5]);
    ptaGetPt(ptad, 3, &b[6], &b[7]);

    for (i = 0; i < 8; i++) {
        if ((a[i] = (l_float32 *)CALLOC(8, sizeof(l_float32))) == NULL)
            return ERROR_INT("a[i] not made", procName, 1);
    }

    a[0][0] = x1;
    a[0][1] = y1;
    a[0][2] = 1.;
    a[0][6] = -x1 * b[0];
    a[0][7] = -y1 * b[0];
    a[1][3] = x1;
    a[1][4] = y1;
    a[1][5] = 1;
    a[1][6] = -x1 * b[1];
    a[1][7] = -y1 * b[1];
    a[2][0] = x2;
    a[2][1] = y2;
    a[2][2] = 1.;
    a[2][6] = -x2 * b[2];
    a[2][7] = -y2 * b[2];
    a[3][3] = x2;
    a[3][4] = y2;
    a[3][5] = 1;
    a[3][6] = -x2 * b[3];
    a[3][7] = -y2 * b[3];
    a[4][0] = x3;
    a[4][1] = y3;
    a[4][2] = 1.;
    a[4][6] = -x3 * b[4];
    a[4][7] = -y3 * b[4];
    a[5][3] = x3;
    a[5][4] = y3;
    a[5][5] = 1;
    a[5][6] = -x3 * b[5];
    a[5][7] = -y3 * b[5];
    a[6][0] = x4;
    a[6][1] = y4;
    a[6][2] = 1.;
    a[6][6] = -x4 * b[6];
    a[6][7] = -y4 * b[6];
    a[7][3] = x4;
    a[7][4] = y4;
    a[7][5] = 1;
    a[7][6] = -x4 * b[7];
    a[7][7] = -y4 * b[7];

    gaussjordan(a, b, 8);

    for (i = 0; i < 8; i++)
        FREE(a[i]);

    return 0;
}


/*!
 *  projectiveXformSampledPt()
 *
 *      Input:  vc (vector of 8 coefficients)
 *              (x, y)  (initial point)
 *              (&xp, &yp)   (<return> transformed point)
 *      Return: 0 if OK; 1 on error
 *
 *  Notes:
 *      (1) This finds the nearest pixel coordinates of the transformed point.
 *      (2) It does not check ptrs for returned data!
 */
l_int32
projectiveXformSampledPt(l_float32  *vc,
                         l_int32     x,
                         l_int32     y,
                         l_int32    *pxp,
                         l_int32    *pyp)
{
l_float32  factor;

    PROCNAME("projectiveXformSampledPt");

    if (!vc)
        return ERROR_INT("vc not defined", procName, 1);

    factor = 1. / (vc[6] * x + vc[7] * y + 1.);
    *pxp = (l_int32)(factor * (vc[0] * x + vc[1] * y + vc[2]) + 0.5);
    *pyp = (l_int32)(factor * (vc[3] * x + vc[4] * y + vc[5]) + 0.5);
    return 0;
}


/*!
 *  projectiveXformPt()
 *
 *      Input:  vc (vector of 8 coefficients)
 *              (x, y)  (initial point)
 *              (&xp, &yp)   (<return> transformed point)
 *      Return: 0 if OK; 1 on error
 *
 *  Notes:
 *      (1) This computes the floating point location of the transformed point.
 *      (2) It does not check ptrs for returned data!
 */
l_int32
projectiveXformPt(l_float32  *vc,
                  l_int32     x,
                  l_int32     y,
                  l_float32  *pxp,
                  l_float32  *pyp)
{
l_float32  factor;

    PROCNAME("projectiveXformPt");

    if (!vc)
        return ERROR_INT("vc not defined", procName, 1);

    factor = 1. / (vc[6] * x + vc[7] * y + 1.);
    *pxp = factor * (vc[0] * x + vc[1] * y + vc[2]);
    *pyp = factor * (vc[3] * x + vc[4] * y + vc[5]);
    return 0;
}

