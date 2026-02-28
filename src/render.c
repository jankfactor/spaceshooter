#include <kernel.h>

#include "render.h"
#include "math3d.h"
#include "mesh.h"
#include "palette.h"

extern void FillEdgeLists(int triList, int color);
extern void ProjectVertex(int vertexPtr);

TRI *g_RenderQueue[MAXDEPTH];
TRI *queuePtr = NULL;
int *gEdgeList = NULL;
extern unsigned int EdgeList;

int gDebug = 0;

#define MAX_EXTRA_VERTS 192
#define MAX_EXTRA_TRIS 64

TRI *clippedQueue1 = 0;
V3D clippedNearVerts[MAX_EXTRA_VERTS];
int clippedNearVertIndex = 0;
TRI clippedNearTris[MAX_EXTRA_TRIS];
int clippedNearTrisIndex = 0;

#ifdef TIMING_LOG
TimerLog gTimerLog;

int GetRenderDelta(void)
{
    _kernel_oserror *err;
    _kernel_swi_regs rin, rout;
    int deltaTime = 0;

    err = _kernel_swi(SWI_Timer_Stop, &rin, &rout);
    deltaTime = rout.r[1];
    err = _kernel_swi(SWI_Timer_Start, &rin, &rout);

    return deltaTime;
}
#endif // TIMING_LOG

void SetupRender(int allocating)
{
    memset(g_RenderQueue, 0, MAXDEPTH * sizeof(TRI *));
    if (allocating)
    {
        cvector_reserve(gEdgeList, 256);
        EdgeList = (unsigned int)(gEdgeList); // For ASM access
    }
    else
    {
        // Free up memory that was allocated
        cvector_free(gEdgeList);
    }
}

#define interpolate(a, b)                                         \
    do                                                            \
    {                                                             \
        t = ((a)->z - (b)->z) >> 8;                               \
        if (t != 0)                                               \
        {                                                         \
            t = ((a)->z << 8) / t;                                \
            tmp.x = (((b)->x - (a)->x) >> 8) * (t >> 8) + (a)->x; \
            tmp.y = (((b)->y - (a)->y) >> 8) * (t >> 8) + (a)->y; \
            tmp.z = 0;                                            \
            inside.verts[inside.numVerts++] = tmp;                \
        }                                                         \
        else                                                      \
        {                                                         \
            (a)->z = 0;                                           \
        }                                                         \
    } while (0);

// void ClipPolyonListToNearPlane(POLYGON *p)
// {
//     static POLYGON inside;
//     static V3D *previous, *current;
//     static V3D tmp;
//     register int i, t;

//     inside.numVerts = 0;
//     previous = &p->verts[p->numVerts - 1];

//     for (i = 0; i < p->numVerts; ++i)
//     {
//         current = &p->verts[i];

//         // Have we crossed into near plane?
//         if ((previous->z ^ current->z) & 0x80000000)
//         {
//             if (previous->z < 0)
//             {
//                 interpolate(current, previous);
//             }
//             else
//             {
//                 interpolate(previous, current);
//             }
//         }

//         if (current->z >= 0)
//             inside.verts[inside.numVerts++] = *current;

//         previous = current;
//     }

//     *p = inside;
// }

void RenderModel(MAT43 *mv, V3D *eyePos, int yaw)
{
    // FillEdgeLists((int)&_verts[0], k);
}
