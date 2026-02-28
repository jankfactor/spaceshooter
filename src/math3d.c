#include "math3d.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "cvector.h"

fix *g_oneOver;              // Reciprocal table (for max screen height of 256 in Mode 13)
extern unsigned int OneOver; // Address of the above table for ASM access
fix *g_SineTable;            // SIN table. Offset used for COS.

void SetupMathsGlobals(int isAllocating)
{
    if (isAllocating)
    {
        printf("Allocating tables required for Math3D...\n");
        cvector_reserve(g_SineTable, SINETABLE_SIZE);
        cvector_reserve(g_oneOver, ONEOVERTABLE_SIZE);
        OneOver = (unsigned int)(g_oneOver);
        printf("Done.\n");
    }
    else
    {
        printf("Freeing tables required for Math3D...\n");
        cvector_free(g_SineTable);
        cvector_free(g_oneOver);
        printf("Done.\n");
    }
}

void SetIdentity(MAT43 *mat)
{
    mat->m11 = 65536;
    mat->m12 = 0;
    mat->m13 = 0;
    mat->m21 = 0;
    mat->m22 = 65536;
    mat->m23 = 0;
    mat->m31 = 0;
    mat->m32 = 0;
    mat->m33 = 65536;
    mat->tx = 0;
    mat->ty = 0;
    mat->tz = 0;
}

void SetScale(MAT43 *mat, fix sx, fix sy, fix sz)
{
    mat->m11 = fixmult(mat->m11, sx);
    mat->m22 = fixmult(mat->m22, sy);
    mat->m33 = fixmult(mat->m33, sz);
}

void SetScaleUniversal(MAT43 *mat, fix s)
{
    SetScale(mat, s, s, s);
}

void EulerToMat(MAT43 *mat, int heading, int pitch, int bank)
{
    fix sh, ch, sp, cp, sb, cb;
    sh = fixsin(heading);
    ch = fixcos(heading);
    sp = fixsin(pitch);
    cp = fixcos(pitch);
    sb = fixsin(bank);
    cb = fixcos(bank);

    mat->m11 = fixmult(ch, cb) + fixmult(fixmult(sh, sp), sb);
    mat->m12 = fixmult(-ch, sb) + fixmult(fixmult(sh, sp), cb);
    mat->m13 = fixmult(sh, cp);

    mat->m21 = fixmult(sb, cp);
    mat->m22 = fixmult(cb, cp);
    mat->m23 = -sp;

    mat->m31 = fixmult(-sh, cb) + fixmult(fixmult(ch, sp), sb);
    mat->m32 = fixmult(sb, sh) + fixmult(fixmult(ch, sp), cb);
    mat->m33 = fixmult(ch, cp);

    mat->tx = mat->ty = mat->tz = 0;
}

void RotateAxis(MAT43 *mat, V3D *axis, int angle)
{
    fix s, c, a, ax, ay, az;
    s = fixsin(angle);
    c = fixcos(angle);
    a = 65536 - c;
    ax = fixmult(a, axis->x);
    ay = fixmult(a, axis->y);
    az = fixmult(a, axis->z);

    mat->m11 = fixmult(ax, axis->x) + c;
    mat->m12 = fixmult(ax, axis->y) + fixmult(s, axis->z);
    mat->m13 = fixmult(ax, axis->z) - fixmult(s, axis->y);
    mat->m21 = fixmult(ay, axis->x) - fixmult(s, axis->z);
    mat->m22 = fixmult(ay, axis->y) + c;
    mat->m23 = fixmult(ay, axis->z) + fixmult(s, axis->x);
    mat->m31 = fixmult(az, axis->x) + fixmult(s, axis->y);
    mat->m32 = fixmult(az, axis->y) - fixmult(s, axis->x);
    mat->m33 = fixmult(az, axis->z) + c;

    mat->tx = mat->ty = mat->tz = 0;
}

void RotateX(MAT43 *mat, int angle)
{
    fix s, c;
    s = fixsin(angle);
    c = fixcos(angle);

    mat->m11 = 65536;
    mat->m12 = 0;
    mat->m13 = 0;
    mat->m21 = 0;
    mat->m22 = c;
    mat->m23 = s;
    mat->m31 = 0;
    mat->m32 = -s;
    mat->m33 = c;
    mat->tx = mat->ty = mat->tz = 0;
}

void RotateY(MAT43 *mat, int angle)
{
    fix s, c;
    s = fixsin(angle);
    c = fixcos(angle);

    mat->m11 = c;
    mat->m12 = 0;
    mat->m13 = -s;
    mat->m21 = 0;
    mat->m22 = 65536;
    mat->m23 = 0;
    mat->m31 = s;
    mat->m32 = 0;
    mat->m33 = c;
    mat->tx = mat->ty = mat->tz = 0;
}

void Normal(V3D *a, V3D *b, V3D *c, V3D *n)
{
    V3D v1, v2;
    v1.x = a->x - b->x;
    v1.y = a->y - b->y;
    v1.z = a->z - b->z;
    v2.x = a->x - c->x;
    v2.y = a->y - c->y;
    v2.z = a->z - c->z;

    n->x = fixmult(v1.y, v2.z) - fixmult(v1.z, v2.y);
    n->y = fixmult(v1.z, v2.x) - fixmult(v1.x, v2.z);
    n->z = fixmult(v1.x, v2.y) - fixmult(v1.y, v2.x);
}

void Normalize(V3D *v)
{
    // All-integer normalize: avoids FP emulator traps on ARM2/3.
    // Compute squared length in 16.16 fixed-point.
    fix sq = fixmult(v->x, v->x) + fixmult(v->y, v->y) + fixmult(v->z, v->z);
    unsigned int n, x, y;

    if (sq <= 0) return;

    // Integer square root (Newton-Raphson).
    // isqrt(sq) = sqrt(real_value * 65536) = sqrt(real_value) * 256,
    // i.e. the length in 8.8 fixed-point representation.
    n = (unsigned int)sq;
    x = n;
    y = (x + 1) >> 1;
    while (y < x) {
        x = y;
        y = (x + n / x) >> 1;
    }

    // x = length * 256 (8.8 representation).
    // Divide each component by length to normalize:
    // v_i (16.16) / length (16.16) → (v_i << 8) / x
    if (x == 0) return;
    v->x = (v->x << 8) / (fix)x;
    v->y = (v->y << 8) / (fix)x;
    v->z = (v->z << 8) / (fix)x;
}

void MultMatMat(MAT43 *dest, MAT43 *a, MAT43 *b)
{
    MAT43 tmp;
    tmp.m11 = fixmult(a->m11, b->m11) + fixmult(a->m12, b->m21) + fixmult(a->m13, b->m31);
    tmp.m12 = fixmult(a->m11, b->m12) + fixmult(a->m12, b->m22) + fixmult(a->m13, b->m32);
    tmp.m13 = fixmult(a->m11, b->m13) + fixmult(a->m12, b->m23) + fixmult(a->m13, b->m33);

    tmp.m21 = fixmult(a->m21, b->m11) + fixmult(a->m22, b->m21) + fixmult(a->m23, b->m31);
    tmp.m22 = fixmult(a->m21, b->m12) + fixmult(a->m22, b->m22) + fixmult(a->m23, b->m32);
    tmp.m23 = fixmult(a->m21, b->m13) + fixmult(a->m22, b->m23) + fixmult(a->m23, b->m33);

    tmp.m31 = fixmult(a->m31, b->m11) + fixmult(a->m32, b->m21) + fixmult(a->m33, b->m31);
    tmp.m32 = fixmult(a->m31, b->m12) + fixmult(a->m32, b->m22) + fixmult(a->m33, b->m32);
    tmp.m33 = fixmult(a->m31, b->m13) + fixmult(a->m32, b->m23) + fixmult(a->m33, b->m33);

    // Translation (column-vector convention: dest_t = A_rot * B_t + A_t)
    tmp.tx = fixmult(a->m11, b->tx) + fixmult(a->m12, b->ty) + fixmult(a->m13, b->tz) + a->tx;
    tmp.ty = fixmult(a->m21, b->tx) + fixmult(a->m22, b->ty) + fixmult(a->m23, b->tz) + a->ty;
    tmp.tz = fixmult(a->m31, b->tx) + fixmult(a->m32, b->ty) + fixmult(a->m33, b->tz) + a->tz;

    *dest = tmp;
}

V3D SubV3D(const V3D *a, const V3D *b)
{
    V3D result;
    result.x = a->x - b->x;
    result.y = a->y - b->y;
    result.z = a->z - b->z;
    return result;
}

void ViewMatrix(const V3D *p, const V3D *rpy, MAT43 *mat)
{
    fix cr = fixcos(rpy->z), sr = fixsin(rpy->z); // roll  Z
    fix cp = fixcos(rpy->x), sp = fixsin(rpy->x); // pitch X
    fix cy = fixcos(rpy->y), sy = fixsin(rpy->y); // yaw   Y

    mat->m11 = fixmult(cr, cy) + fixmult(fixmult(sr, sp), sy);
    mat->m12 = fixmult(sr, cp);
    mat->m13 = fixmult(-cr, sy) + fixmult(fixmult(sr, sp), cy);

    mat->m21 = fixmult(-sr, cy) + fixmult(fixmult(cr, sp), sy);
    mat->m22 = fixmult(cr, cp);
    mat->m23 = fixmult(sr, sy) + fixmult(fixmult(cr, sp), cy);

    mat->m31 = fixmult(cp, sy);
    mat->m32 = -sp;
    mat->m33 = fixmult(cp, cy);

    mat->tx = p->x;
    mat->ty = p->y;
    mat->tz = p->z;
}

void LookAt(const V3D *eyePos, const V3D *forward, MAT43 *mat)
{
    // Calculate the forward vector (direction from eye to target)
    V3D up, right;

    // Define the up vector (world's up)
    up.x = 0;
    up.y = 65536;
    up.z = 0;

    // Calculate the right vector (perpendicular to forward and up)
    right.x = fixmult(up.y, forward->z) - fixmult(up.z, forward->y);
    right.y = fixmult(up.z, forward->x) - fixmult(up.x, forward->z);
    right.z = fixmult(up.x, forward->y) - fixmult(up.y, forward->x);
    up.x = fixmult(forward->y, right.z) - fixmult(forward->z, right.y);
    up.y = fixmult(forward->z, right.x) - fixmult(forward->x, right.z);
    up.z = fixmult(forward->x, right.y) - fixmult(forward->y, right.x);

    mat->tx = -DotProduct(&right, eyePos);
    mat->ty = -DotProduct(&up, eyePos);
    mat->tz = -DotProduct(forward, eyePos);

    // Fill in the matrix values
    mat->m11 = right.x;
    mat->m12 = right.y;
    mat->m13 = right.z;

    mat->m21 = up.x;
    mat->m22 = up.y;
    mat->m23 = up.z;

    mat->m31 = forward->x;
    mat->m32 = forward->y;
    mat->m33 = forward->z;
}

void PerspectiveProjection(MAT44 *mat, float fov, float aspect, float znear, float zfar)
{
    float yScale = 1.f / tanf(fov / 2.f);
    float xScale = yScale * aspect;

    mat->m11 = float2fix(xScale);
    mat->m12 = 0;
    mat->m13 = 0;
    mat->m14 = 0;

    mat->m21 = 0;
    mat->m22 = float2fix(yScale);
    mat->m23 = 0;
    mat->m24 = 0;

    mat->m31 = 0;
    mat->m32 = 0;
    mat->m33 = float2fix(zfar / (zfar - znear));
    mat->m34 = 65535;

    mat->m41 = 0;
    mat->m42 = 0;
    mat->m43 = float2fix(znear * zfar / (zfar - znear));
    mat->m44 = 0;
}
